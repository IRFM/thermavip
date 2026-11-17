"""An IPython QWidget for an existing Qt application and Python interpreter.

Select the host's Qt binding before importing this module (e.g. QT_API=pyside6).
Construct and use the widget on the GUI thread. Use one console per host session.
"""

import asyncio
import atexit
from concurrent.futures import ThreadPoolExecutor
from contextlib import closing
import logging
from threading import Event, Lock
from pathlib import Path
import sqlite3
import sys
from types import ModuleType

from ipykernel.inprocess.ipkernel import InProcessInteractiveShell, InProcessKernel

from qtpy.QtCore import QStandardPaths, Signal
from qtpy.QtGui import QColor, QTextCharFormat
from qtpy.QtWidgets import QApplication
from qtconsole.inprocess import QtInProcessChannel, QtInProcessKernelClient, QtInProcessKernelManager
from qtconsole.rich_jupyter_widget import RichJupyterWidget
from traitlets import Type
from traitlets.config import Config


class _ModuleKernelManager(QtInProcessKernelManager):
    """Start the kernel with the host module before IPython initializes its shell."""

    def __init__(self, module, **kwargs):
        super().__init__(**kwargs)
        self._module = module

    def start_kernel(self, **kwargs):
        self.kernel = InProcessKernel(
            parent=self, session=self.session, user_module=self._module,
            user_ns=self._module.__dict__,
        )


class _WorkerShell(InProcessInteractiveShell):
    cancel_event = None

    async def run_code(self, code_obj, result=None, *, async_=False):
        """Deliver a requested interrupt inside IPython's user-code handler."""
        token = self.cancel_event
        if token is None:
            return await super().run_code(code_obj, result, async_=async_)

        def trace(frame, event, arg):
            if event == "call":
                if frame.f_code is InProcessKernel._input_request.__code__:
                    return None  # The stdin channel has an explicit wakeup.
                parent = frame.f_back
                if frame.f_code is not code_obj and (parent is None or parent.f_trace is not trace):
                    return None
            if token.is_set():
                token.clear()
                raise KeyboardInterrupt
            return trace

        previous = sys.gettrace()
        try:
            sys.settrace(trace)
            return await super().run_code(code_obj, result, async_=async_)
        finally:
            sys.settrace(previous)

    def init_sys_modules(self):
        # The host owns sys.modules["__main__"].
        pass


class _WorkerKernel(InProcessKernel):
    shell_class = Type(_WorkerShell)


    # Python permits signal handlers only on the main thread.
    def pre_handler_hook(self):
        pass

    def post_handler_hook(self):
        pass


class _ReplyChannel(QtInProcessChannel):
    def call_handlers_later(self, msg):
        # The worker has no Qt event loop. Deliver to the GUI via a Qt signal.
        self.message_received.emit(msg)


class _InputChannel(_ReplyChannel):
    def process_events(self):
        # ipykernel calls this while waiting for input(). Sleep until the GUI
        # supplies the reply or shuts down; never pump Qt events on the worker.
        client = self.client
        if client.closing:
            raise EOFError("Console closed")
        client.input_ready.wait()
        client.input_ready.clear()
        token = client.kernel.shell.cancel_event
        if token is not None and token.is_set():
            token.clear()
            raise KeyboardInterrupt
        if client.closing:
            raise EOFError("Console closed")


class _WorkerClient(QtInProcessKernelClient):
    shell_channel_class = Type(_ReplyChannel)
    stdin_channel_class = Type(_InputChannel)
    dispatch_failed = Signal(str)

    def _dispatch_to_kernel(self, msg):
        if self.closing:
            return
        token = None
        if msg["header"]["msg_type"] == "execute_request" and not msg["content"].get("silent"):
            token = Event()
            with self.pending_lock:
                self.pending.append(token)
        self.executor.submit(self._dispatch_on_worker, msg, token)

    def request_stop(self):
        with self.pending_lock:
            if not self.pending:
                return False
            self.pending[0].set()
            self.input_ready.set()
            return True

    def _dispatch_on_worker(self, msg, token):
        self.kernel.shell.cancel_event = token
        try:
            super()._dispatch_to_kernel(msg)
        except Exception as exc:
            logging.getLogger(__name__).exception("Kernel dispatch failed")
            self.dispatch_failed.emit(str(exc))
        finally:
            self.kernel.shell.cancel_event = None
            if token is not None:
                with self.pending_lock:
                    self.pending.remove(token)

    def input(self, string):
        super().input(string)
        self.input_ready.set()


class EmbeddedIPython(RichJupyterWidget):
    """A QtConsole widget backed by a kernel in the calling interpreter."""

    dark_style = f"""
            QPlainTextEdit, QTextEdit {{
                background-color: #252621;
                color: #f2f2f2;
                selection-background-color: #496582;
                selection-color: white;
            }}
            .in-prompt {{ color: #9bd89d; }}
            .out-prompt {{ color: #ffac92; }}
            .in-prompt-number, .out-prompt-number {{ font-weight: bold; }}
            .error {{ color: #ff9090; }}
        """
    gray_style = f"""
            QPlainTextEdit, QTextEdit {{
                background-color: #E0E0E0;
                color: black;
                selection-background-color: #496582;
                selection-color: white;
            }}
            .in-prompt {{ color: green; }}
            .out-prompt {{ color: black; }}
            .in-prompt-number, .out-prompt-number {{ font-weight: bold; }}
            .error {{ color: red; }}
            
        """
    _styles = {"dark": dark_style, "gray": gray_style}

    def __init__(self, parent=None, *, entries=None, scheme="dark", history_file=None, global_ns=None, threaded=True):
        if QApplication.instance() is None:
            raise RuntimeError("Create the host QApplication first")
        super().__init__(parent=parent)
        self.set_color_scheme(scheme)

        if history_file is None:
            location = QStandardPaths.writableLocation(
                QStandardPaths.StandardLocation.AppDataLocation
            )
            if not location:
                raise RuntimeError("Qt could not locate the application data directory")
            history_file = Path(location) / "ipython" / "history.sqlite"
        self._history_file = Path(history_file).expanduser().resolve()
        self._history_file.parent.mkdir(parents=True, exist_ok=True)

        self._host_module = None
        if global_ns is not None:
            if not isinstance(global_ns, dict):
                raise TypeError("global_ns must be a module globals() dictionary")
            module = sys.modules.get(global_ns.get("__name__"))
            if not isinstance(module, ModuleType) or vars(module) is not global_ns:
                raise ValueError("Pass globals() from a module registered in sys.modules")
            self._host_module = module

        self.threaded = threaded
        if threaded:
            self.custom_restart = True
            self.custom_restart_requested.connect(self._request_worker_restart)
        self._host_entries = {}
        self._stop = None
        self._start_session()
        try:
            if entries is not None:
                self.add_entries(entries)
        except BaseException:
            self.shutdown()
            raise
        QApplication.instance().aboutToQuit.connect(self.shutdown)

    def _start_session(self):
        config = Config()
        config.HistoryManager.enabled = True
        config.HistoryManager.hist_file = str(self._history_file)
        if self.threaded:
            self._start_worker_session(config)
            return
        if self._host_module is None:
            manager = QtInProcessKernelManager(config=config)
        else:
            manager = _ModuleKernelManager(self._host_module, config=config)
        manager.start_kernel()
        shell = manager.kernel.shell
        client = manager.client()
        shared = self._host_module is not None
        stopped = False

        # This callback also works during QObject destruction: it does not
        # dereference the widget after its C++ instance has been destroyed.
        def stop(*_):
            nonlocal stopped
            if stopped:
                return
            stopped = True
            try:
                client.stop_channels()
            finally:
                try:
                    try:
                        if shared:
                            # A retiring shell may reset its namespaces during teardown.
                            # Detach the host dictionary before that can happen.
                            shell.user_module = ModuleType("__console_closed__")
                            shell.user_ns = vars(shell.user_module)
                            shell.ns_table.update(user_global=shell.user_ns, user_local=shell.user_ns)
                        shell.history_manager.end_session()
                    finally:
                        manager.shutdown_kernel()
                finally:
                    # IPython keeps the interactive shell as a process singleton.
                    type(shell).clear_instance()

        self._stop = stop
        try:
            history = shell.history_manager
            if not history.enabled or Path(history.hist_file) != self._history_file:
                raise RuntimeError(f"IPython could not use history file {self._history_file}")
            manager.kernel.gui = "qt"
            manager.kernel.shell.run_line_magic("colors", "linux")
            self.kernel_manager = manager
            self.kernel_client = client
            shell.push(self._host_entries)
            client.start_channels()
            self.destroyed.connect(stop)
        except BaseException:
            stop()
            raise

    def _start_worker_session(self, config):
        manager = QtInProcessKernelManager(config=config)
        executor = ThreadPoolExecutor(max_workers=1, thread_name_prefix="EmbeddedIPython")
        module = self._host_module
        entries = self._host_entries.copy()
        path = self._history_file
        session = manager.session

        def retire(kernel):
            shell = kernel.shell
            if module is not None:
                shell.user_module = ModuleType("__console_closed__")
                shell.user_ns = vars(shell.user_module)
                shell.ns_table.update(user_global=shell.user_ns, user_local=shell.user_ns)
            try:
                # Run IPython cleanup on its owning thread while SQLite is available.
                atexit.unregister(shell.atexit_operations)
                shell.atexit_operations()
            finally:
                kernel.iopub_thread.stop()
                type(shell).clear_instance()

        def start():
            loop = asyncio.new_event_loop()
            asyncio.set_event_loop(loop)
            kernel = None
            try:
                kwargs = {} if module is None else dict(user_module=module, user_ns=vars(module))
                kernel = _WorkerKernel(config=config, session=session, **kwargs)
                shell = kernel.shell
                history = shell.history_manager
                if not history.enabled or Path(history.hist_file) != path:
                    raise RuntimeError(f"IPython could not use history file {path}")
                shell.run_line_magic("colors", "linux")
                shell.push(entries)
                shell.events.register("post_run_cell", lambda *_: history.writeout_cache())
                return kernel, loop
            except BaseException:
                if kernel is not None:
                    retire(kernel)
                loop.close()
                raise

        try:
            kernel, loop = executor.submit(start).result()
        except BaseException:
            executor.shutdown()
            raise
        manager.kernel = kernel
        client = _WorkerClient(kernel=kernel, session=manager.session.clone())
        client.executor = executor
        client.pending_lock = Lock()
        client.pending = []
        client.input_ready = Event()
        client.closing = False

        def finish():
            try:
                retire(kernel)
            finally:
                loop.close()

        def stop(*_):
            if client.closing:
                return
            client.closing = True
            client.input_ready.set()
            try:
                # Drain outstanding work before destroying channels or Python.
                executor.submit(finish).result()
            finally:
                executor.shutdown()
                client.stop_channels()
                manager.kernel = None

        self._stop = stop
        try:
            self.kernel_manager = manager
            self.kernel_client = client
            client.dispatch_failed.connect(self._dispatch_error)
            client.start_channels()
            self.destroyed.connect(stop)
        except BaseException:
            stop()
            raise

    def _request_worker_restart(self):
        if self._executing or self._reading:
            self.append_output("Restart is available when the cell finishes.")
        else:
            self.restart_session()

    def stop_current_code(self):
        """Request KeyboardInterrupt in the current threaded cell; False if idle."""
        if not self.threaded:
            return False
        if self._stop is None:
            return False
        return self.kernel_client.request_stop()

    def interrupt_kernel(self):
        if self.threaded:
            self.stop_current_code()
        else:
            super().interrupt_kernel()

    def _dispatch_error(self, message):
        self.append_output("Kernel request failed: " + message)

    def restart_session(self):
        """Restart the in-process kernel from the GUI thread at an idle prompt.

        Private sessions restore registered objects. Shared sessions keep host globals.
        """
        if self._stop is None:
            raise RuntimeError("The console has been shut down")
        if self._executing or self._reading:
            raise RuntimeError("Wait for the console to return to its prompt")
        self.destroyed.disconnect(self._stop)
        self.shutdown()
        self.kernel_client = None
        self.kernel_manager = None
        try:
            self._start_session()
        except BaseException:
            self._stop = None
            raise
        self.setEnabled(True)

    def get_history_filename(self):
        """Return the absolute path to the SQLite history file as a string."""
        return str(self._history_file)

    def get_history_lines(self, limit=None):
        """Return raw executed cells across sessions, oldest first.

        A multiline cell is one string. A limit selects the latest N cells.
        Works after shutdown as well as while the kernel is running.
        """
        if limit is not None:
            if isinstance(limit, bool) or not isinstance(limit, int):
                raise TypeError("limit must be an integer or None")
            if limit < 0:
                raise ValueError("limit must be nonnegative")
        if self._stop is not None and not self.threaded:
            self.kernel_manager.kernel.shell.history_manager.writeout_cache()
        uri = self._history_file.as_uri() + "?mode=ro"
        with closing(sqlite3.connect(uri, uri=True)) as db:
            if limit is None:
                rows = db.execute("SELECT source_raw FROM history ORDER BY session, line")
                return [source for (source,) in rows]
            rows = db.execute(
                "SELECT source_raw FROM history ORDER BY session DESC, line DESC LIMIT ?",
                (limit,),
            )
            return [source for (source,) in rows][::-1]

    def add_entries(self, entries):
        """Add or replace names in the live IPython namespace from a mapping."""
        values = dict(entries)
        if not all(isinstance(name, str) for name in values):
            raise TypeError("Namespace keys must be strings")
        if self.threaded:
            # Ordered with subsequent execute/complete requests. Never wait for
            # a busy cell from the GUI just to expose another object.
            self.kernel_client.executor.submit(self.kernel_manager.kernel.shell.push, values)
        else:
            self.kernel_manager.kernel.shell.push(values)
        if self._host_module is None:
            self._host_entries.update(values)

    def add_entry(self, name, value):
        """Expose one object by reference, including a wrapped QObject."""
        self.add_entries({name: value})

    def paste_code(self, code, *, append=False):
        """Place code in the editable prompt; pass append=True to keep its draft.

        Call while the console is at an input prompt. The user can edit or
        submit the cell normally. Appending preserves the supplied whitespace.
        """
        if not isinstance(code, str):
            raise TypeError("code must be a string")
        code = code.replace("\r\n", "\n").replace("\r", "\n")
        self.input_buffer = (self.input_buffer if append else "") + code
        self.setFocus()

    def append_output(self, text, color=None):
        """Display a plain-text message above the prompt, optionally colored."""
        if not isinstance(text, str):
            raise TypeError("Output must be a string")
        if not text:
            return
        text = text.replace("\r\n", "\n").replace("\r", "\n")
        if not text.endswith("\n"):
            text += "\n"
        if color is None:
            self._append_plain_text(text, before_prompt=True)
            return
        foreground = QColor(color)
        if not foreground.isValid():
            raise ValueError(f"Invalid output color: {color!r}")

        def insert_message(cursor, message):
            original = cursor.charFormat()
            colored = QTextCharFormat(original)
            colored.setForeground(foreground)
            cursor.insertText(message[:-1], colored)
            cursor.insertText("\n", original)

        self._append_custom(insert_message, text, before_prompt=True)

    def add_input_lines(self, lines):
        """Append strings to the current editable cell, one per line.

        Each argument can also be a multiline block. Call on the GUI thread
        at an idle prompt. Call execute() separately to submit the cell.
        """
        if not lines:
            return
        if not all(isinstance(line, str) for line in lines):
            raise TypeError("Each input line must be a string")
        if self._stop is None or self._executing or self._reading:
            raise RuntimeError("The console must be at an idle input prompt")

        draft = self.input_buffer
        separator = "\n" if draft and not draft.endswith("\n") else ""
        self.paste_code(separator + "\n".join(lines), append=True)

    def set_color_scheme(self, scheme):
        """Apply the 'dark' or 'gray' palette to the console only."""
        if scheme not in self._styles:
            raise ValueError("scheme must be 'dark' or 'gray'")
        
        if scheme == 'dark':
            self.syntax_style = "monokai"
        else:
            self.syntax_style = "xcode"
        self.style_sheet = self._styles[scheme]
    
    def scroll_to_end(self):
        """Reveal the end of the console transcript without moving the input cursor."""
        self._flush_pending_stream()
        self._control.document().size()  # Complete any deferred QTextEdit layout.
        self._scroll_to_end()
        

    def shutdown(self):
        """Stop the console before finalizing the embedded interpreter."""
        if self._stop is not None:
            self._stop()
            self._stop = None
        self.setEnabled(False)
