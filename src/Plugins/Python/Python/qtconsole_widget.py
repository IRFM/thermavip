"""An example of embedding a RichJupyterWidget with an in-process kernel.
We recommend using a kernel in a separate process as the normal option - see
embed_qtconsole.py for more information. In-process kernels are not well
supported.
To run this example:
    python3 inprocess_qtconsole.py
"""


from qtpy import QtWidgets
from qtpy import QtCore as core
import qtconsole
from qtconsole import styles, __version__
from qtconsole.rich_jupyter_widget import RichJupyterWidget
from qtconsole.jupyter_widget import JupyterWidget
from qtconsole.console_widget import ConsoleWidget
from qtconsole.inprocess import QtInProcessKernelManager

#correct bug: https://github.com/jupyter/notebook/issues/4613
# import asyncio
# import sys
# if sys.platform == 'win32':
    # asyncio.set_event_loop_policy(asyncio.WindowsSelectorEventLoopPolicy())
import os    
import ctypes
import threading
import time
import subprocess
import platform
import re


if os.name != 'nt':
    def pid_exists(pid):
        """Check whether pid exists in the current process table."""
        import errno
        if pid < 0:
            return False
        try:
            os.kill(pid, 0)
        except OSError as e:
            return e.errno == errno.EPERM
        else:
            return True
else:
    def pid_exists(pid):
        import ctypes
        kernel32 = ctypes.windll.kernel32
        SYNCHRONIZE = 0x100000

        process = kernel32.OpenProcess(SYNCHRONIZE, 0, pid)
        if process != 0:
            kernel32.CloseHandle(process)
            return True
        else:
            return False



def pid_alive(pid:int):
    """ Check For whether a pid is alive """


    system = platform.uname().system
    if re.search('Linux', system, re.IGNORECASE):
        try:
            os.kill(pid, 0)
        except OSError:
            return False
        else:
            return True
    elif re.search('Windows', system, re.IGNORECASE):
        out = subprocess.check_output(["tasklist","/fi",f"PID eq {pid}"]).strip()
        # b'INFO: No tasks are running which match the specified criteria.'

        if out.find(str(pid).encode('ascii')) >= 0:
            return True
        else:
            return False
    else:
        raise RuntimeError(f"unsupported system={system}")


main_thread_id = threading.current_thread().ident
shell_thread_id = 0
_inside_process_events=False

class SyncWrite:
    
    def __init__(self,shell):
        self.shell = shell
        self.init_write = self.shell.write
        self.stopth = False
        self.code_obj = None
        self.res = None
        
    def __dell__(self):
    
        self.stopth=True
        
    def __call__(self,text):
        core.QCoreApplication.processEvents()
        return self.init_write(text)
    
class SyncRun:
    
    def __init__(self,shell):
        self.shell = shell
        self.init_run_code = self.shell.run_code
        
        
    def __call__(self,code_obj, result=None, *, async_=False):
        sys.__stdout__.write("start proc events...\n");sys.__stdout__.flush()
        global _inside_process_events
        if not _inside_process_events:
            _inside_process_events = True
            core.QCoreApplication.processEvents()
            _inside_process_events = False
        sys.__stdout__.write("end\n");sys.__stdout__.flush()
        return self.init_run_code(code_obj,result,async_ = async_)
        

    
class AsyncRun(threading.Thread):
    
    def __init__(self,shell):
        super(AsyncRun, self).__init__()
        self.shell = shell
        self.init_run_code = self.shell.run_code
        self.stopth = False
        self.code_obj = None
        self.res = None
        self.start()
        
    def __dell__(self):
        self.stopth=True
        self.join()
        
    def __call__(self,code_obj, result=None, *, async_=False):
        global _inside_process_events
        self.code_obj = code_obj
        self.result = result
        self.async_ = async_
        while self.code_obj:
            if not _inside_process_events:
                _inside_process_events = True
                #sys.__stdout__.write("start proc events...\n");sys.__stdout__.flush()
                #QtWidgets.QApplication.instance().processEvents()
                time.sleep(0.01)
                #sys.__stdout__.write("done\n");sys.__stdout__.flush()
                _inside_process_events = False
        return self.res
        
    def run(self):
        sys.__stdout__.write("start run\n");sys.__stdout__.flush()
        while self.stopth == False:
            try:
                if self.code_obj:
                    #sys.__stdout__.write("start code\n");sys.__stdout__.flush()
                    self.res = self.init_run_code(self.code_obj,self.result,async_ = self.async_)
                    #sys.__stdout__.write("end code\n");sys.__stdout__.flush()
                    self.code_obj = None
                    
                time.sleep(0.001)
            except:
                sys.__stdout__.write("catch exception\n");sys.__stdout__.flush()
                self.code_obj = None
                return None
        sys.__stdout__.write("end run\n");sys.__stdout__.flush()
        


# local trace function which returns itself 
_in_process=True
_last_time = time.time()
def my_tracer(frame, event, arg = None): 
    global _last_time
    global _in_process
    if not _in_process:
        t = time.time()
        if t - _last_time > 0.01:
            _last_time = t
            _in_process=True
            QtWidgets.QApplication.instance().processEvents()
            _in_process=False
    return my_tracer
    
class CustomExec:

    def __init__(self, init_exec):
        self.init_exec = init_exec
        self.running=False
        
    def __call__(self,code,silent = False, store_history = True,user_expressions = {}, allow_stdin = None):
        global _in_process
        _in_process = False
        self.running=True
        ret= self.init_exec( code,silent,store_history,user_expressions,allow_stdin)
        self.running=False
        _in_process = True
        return ret
        


class IPythonInterpreter(RichJupyterWidget):
    class Pull:

        def __ini__(self):

            self.name = None
            self.res = None
            
        def pull(self,globs):
            try:
                self.res = globs[self.name]
            except:
                self.res = None
                
    def _ctype_async_raise(self,target_tid, exception):
        ret = ctypes.pythonapi.PyThreadState_SetAsyncExc(ctypes.c_long(target_tid), ctypes.py_object(exception))
        # ref: http://docs.python.org/c-api/init.html#PyThreadState_SetAsyncExc
        if ret == 0:
            raise ValueError("Invalid thread ID")
        elif ret > 1:
            # Huh? Why would we notify more than one threads?
            # Because we punch a hole into C level interpreter.
            # So it is better to clean up the mess.
            ctypes.pythonapi.PyThreadState_SetAsyncExc(target_tid, NULL)
            raise SystemError("PyThreadState_SetAsyncExc failed")
    
    def _throw_in_main_thread(self):
        self._ctype_async_raise(main_thread_id,KeyboardInterrupt )
        #self._ctype_async_raise(shell_thread_id,KeyboardInterrupt )
        #self._ctype_async_raise(self.kernel.iopub_thread.ident,KeyboardInterrupt)

            
    def __init__(self, fonst_size=11, syntax_style = '',*args,**kwargs):
    
        if fonst_size > 0:
            ConsoleWidget.font_size=fonst_size
        JupyterWidget.syntax_style=syntax_style #'monokai' for dark
        JupyterWidget.syntax_styleUnicode=syntax_style
        
        super(IPythonInterpreter, self).__init__(*args,**kwargs)
        
        if syntax_style:
            if styles.dark_style(syntax_style):
                colors='linux'
            else:
                colors='lightbg'
            self.style_sheet = styles.sheet_from_template(syntax_style,colors)
            self.syntax_style = syntax_style
            self._ansi_processor.set_background_color(self.syntax_style)
            self._syntax_style_changed()
            self._style_sheet_changed()
            #self._ansi_processor.set_background_color(self.syntax_style)
            ch = self.findChildren(QtWidgets.QTextEdit)
            for c in ch:
                c.setStyleSheet(self.styleSheet())
            
        
        self.kernel_manager = QtInProcessKernelManager()
        self.kernel_manager.start_kernel(show_banner=False)
        self.kernel = self.kernel_manager.kernel
        self.kernel.gui = 'qt'
        
        # use a separate thread for execution
        shell = self.kernel.shell
        #shell.run_code = self.asyncrun = AsyncRun(shell)
        #shell.run_code = SyncRun(shell)
        self.asyncrun=None
        
        self.kernel_client = self.kernel_manager.client()
        self.kernel_client.start_channels()
        self.kernel_client.execute = CustomExec(self.kernel_client.execute)
        
        
        self.puller = IPythonInterpreter.Pull()
        self.puller.name = self.puller.res = None
        self.pushObjects({'__puller':self.puller})
        self.pushObjects({'__writer':SyncWrite(shell)})
        self.pushObjects({'__interp':self})
        self.execInKernel("import sys;__writer.init_write = sys.stdout.write;sys.stdout.write=__writer")
        self.execInKernel("import threading;__puller.shell_thread_id = threading.current_thread().ident")
        global shell_thread_id
        shell_thread_id = self.puller.shell_thread_id
        
    def __del__(self):
        if self.asyncrun:
            self.asyncrun.stopth = True
            self.asyncrun.join()
        if self.kernel_client:
            self.kernel_client.stop_channels()
        if self.kernel_manager:
            self.kernel_manager.shutdown_kernel()

    
    def isRunningCode(self):
        return self.kernel_client.execute.running
        
    def closeEvent(self, evt):
        if self.asyncrun:
            self.asyncrun.stopth = True
            self.asyncrun.join()
        if self.kernel_client:
            self.kernel_client.stop_channels()
        if self.kernel_manager:
            self.kernel_manager.shutdown_kernel()

        super(IPythonInterpreter, self).closeEvent(evt)
        
    def clear(self):
        """
        Clears the terminal
        """
        self._control.clear()
    
    def push(self,name, obj):
        self.pushObjects({name:obj})
    
    def pull(self,name):
        self.puller.name = name
        self.execInKernel("__puller.pull(globals()); ")
        return self.puller.res
            
        
    def execLine(self, code):
        """Add entry to interpreter"""
        self.execute(code)
        
    def execInKernel(self,code):
        """Exec code in kernel directly"""
        self.kernel_client.execute(code,silent=True, store_history=False)
        
    def pushObjects(self,objects):
        """Add objects to the kernel. Must be a dict."""
        self.kernel.shell.push(objects)
    
    def restart(self):
        
        async_run = False
        if self.asyncrun:
            async_run = True
            print("restart asyncrun...")
            self.asyncrun.stopth=True
            self.asyncrun.join()
            self.asyncrun = None
            print("done")
        self.kernel_client.stop_channels()
        self.kernel_manager.shutdown_kernel()
        self.kernel_client = None
        self.kernel_manager = None
        self.kernel = None
        
        self.kernel_manager = QtInProcessKernelManager()
        self.kernel_manager.start_kernel(show_banner=False)
        self.kernel = self.kernel_manager.kernel
        self.kernel.gui = 'qt'
        
        # use a separate thread for execution
        shell = self.kernel.shell
        if async_run:
            shell.run_code = self.asyncrun = AsyncRun(shell)
        else:
            self.asyncrun=None
        
        self.kernel_client = self.kernel_manager.client()
        self.kernel_client.start_channels()
        self.kernel_client.execute = CustomExec(self.kernel_client.execute)
        
        self.puller = IPythonInterpreter.Pull()
        self.puller.name = self.puller.res = None
        self.pushObjects({'__puller':self.puller})
        self.pushObjects({'__writer':SyncWrite(shell)})
        self.pushObjects({'__interp':self})
        self.execInKernel("import sys;__writer.init_write = sys.stdout.write;sys.stdout.write=__writer")
        
        
    def stopCode(self):
        """Stop current code from being executed"""
        self._throw_in_main_thread()
        
    def checkParentProcess(self):
        
        if _ppid > 0 and not pid_exists(_ppid):
            sys.exit(-1)



def loop():
    while True:
        QtWidgets.QApplication.instance().processEvents()
        time.sleep(0.1)

#parent pid
_ppid = 0
  
if __name__ == "__main__":
    import sys
    
    app = QtWidgets.QApplication([])
    size = int(sys.argv[1])
    style = sys.argv[2]
    code = sys.argv[3]
    path = sys.argv[4]
    _ppid = int(sys.argv[5])
    if style == 'default':
        style = str()
    interp = IPythonInterpreter(size,style)
    os.chdir(path)
    interp.execInKernel(code)
    print(int(interp.winId()))
    print(_ppid)
    print(threading.current_thread().ident)
    sys.stdout.flush()
    
    embedded = False
    if len(sys.argv) >6:
        embedded = int(sys.argv[6])
        if embedded:
            interp.hide()
            timer = core.QTimer()
            timer.setSingleShot(True)
            timer.setInterval(500)
            timer.timeout.connect(interp.showFullScreen)
            timer.start()
    
    if not embedded:
        interp.show()
    
    # timer2 = core.QTimer()
    # timer2.setSingleShot(True)
    # timer2.setInterval(7000)
    # timer2.timeout.connect(interp.stopCode)
    # timer2.start()
    # interp.execLine(\
# """
# def test():
 # import time
 # t = time.time()
 # i=0
 # while True:
  # i+=1
  # if time.time() - t > 1:
   # return i
# """
# )
    
    sys.settrace(my_tracer) 
    #threading.Thread(target = loop, args=()).start()

    timerPid = core.QTimer()
    timerPid.setSingleShot(False)
    timerPid.setInterval(1000)
    timerPid.timeout.connect(interp.checkParentProcess)
    timerPid.start()
    
    # timer = core.QTimer()
    # timer.setSingleShot(False)
    # timer.setInterval(2000)
    # timer.timeout.connect(interp.restart)
    # timer.start()
    # code = """
    # import time
    # t=time.time()
    # while True:
      # if time.time()-t > 1.0:
          # print(t)
          # t=time.time()
          # sys.stdout.flush()
          # sys.__stdout__.write(str(t));sys.__stdout__.flush()
    # """
    # interp.execLine(code)
    
    
    # import time
    # import numpy as np
    # ar = np.zeros((1000,1000),dtype=np.uint32)
    
    # st = time.time()
    # for i in range (1000):
    #     interp.pushObjects({'ar':ar})
    # el = time.time()-st
    # print('el:',el)
    # st = time.time()
    # for i in range (1000):
    #     ar = interp.pull('ar')
    # el = time.time()-st
    # print('el:',el)
    
    app.exec_()