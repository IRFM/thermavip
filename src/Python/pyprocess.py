# -*- coding: utf-8 -*-
"""
Created on Sun Dec 17 17:34:27 2017

@author: Moncada
"""
import traceback
import struct
import numpy
import time
import sys
import os
import socket
import mmap

current_milli_time = lambda: int(round(time.time() * 1000))
#
#with open("testf","wb") as f:
#    f.write(bytes(1000000))
#    f.flush()
#    f.close()
#    
#
#with open("testf","rb") as f:
#    m1 = mmap.mmap(f.fileno(),0)
#    m1.write(bytes(1000000))
#    f.close()
#with open("testf","rb") as f:   
#    m2 = mmap.mmap(f.fileno(),0)
#    tmp = m2.read(bytes(1000000))
#    f.close()
#




PY_CODE_ERROR = 0
PY_CODE_INT =1
PY_CODE_LONG =2
PY_CODE_DOUBLE =3
PY_CODE_COMPLEX =4
PY_CODE_STRING =5
PY_CODE_BYTES =6
PY_CODE_LIST =7
PY_CODE_DICT =8
PY_CODE_POINT_VECTOR =9
PY_CODE_COMPLEX_POINT_VECTOR =10
PY_CODE_INTERVAL_SAMPLE_VECTOR =11
PY_CODE_NDARRAY =12
PY_CODE_NONE = 13



def __debug(text):
    try:
        f.write(text + "\n")
        f.flush()
    except:
        pass
    

#__stdin = os.fdopen(sys.__stdin__.fileno(),"rb",10000)
def readInput(l): 
    #return __stdin.read(l) 
    
    return sys.__stdin__.buffer.read(l) if sys.version_info[0]==3 else sys.__stdin__.read(l)
    #return sys.__stdin__.buffer.read(l)

#__stdout = os.fdopen(sys.__stdout__.fileno(),"wb",10000)
def writeOutput(data):
    if sys.version_info[0]==3:
        sys.__stdout__.buffer.write(data)
    else:
         sys.__stdout__.write(data)
    sys.__stdout__.flush()
    #__stdout.write(data)
    #__stdout.flush()
    

#mySocket = socket.socket()
#mySocket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
#mySocket.bind(("127.0.0.1",5100))
#mySocket.listen(1)
#client_socket, client_address = mySocket.accept() # blocking call    
#def readInput(l): 
#    res = bytes()
#    while len(res) < l:
#        res += client_socket.recv(l-len(res))
#    return res
#def writeOutput(data): 
#    totalsent = 0
#    while totalsent < len(data):
#        sent = client_socket.send(data[totalsent: totalsent + min(2048*32,len(data)-totalsent)])
#        if sent == 0:
#            raise RuntimeError("socket connection broken")
#        totalsent = totalsent + sent
    


def format_array(ar):
    """
    Prepare a numpy array to be serialized.
    Returns a tuple (array, type)
    """
    kind = ar.dtype.kind
    if kind == 'b' or kind == 'i' or kind == 'u' or kind == 'f':
        return (ar,ar.dtype.char)
    elif kind == 'c':
        return (numpy.array(ar,dtype='D'),'D')
    else:
        return (None,None)

def pyToBytes(obj):
    """
    Serialize input object and return its bytes representation
    """
    if sys.version_info[0]==2:
        if isinstance(obj, (unicode)):
            val = obj.encode('utf-16le')
            return bytes(struct.pack('<i',PY_CODE_STRING)) + bytes(struct.pack('<i',len(val))) +val
        elif isinstance(obj, (str)):
            return bytes(struct.pack('<i',PY_CODE_BYTES)) + bytes(struct.pack('<i',len(obj))) + obj
    

    if isinstance(obj, (int)):
        return bytes(struct.pack('<i',PY_CODE_LONG)) + bytes(struct.pack('<q',int(obj)))
    elif isinstance(obj, (float)):
        return bytes(struct.pack('<i',PY_CODE_DOUBLE)) + bytes(struct.pack('<d',float(obj)))
    elif isinstance(obj, (str)):
        return bytes(struct.pack('<i',PY_CODE_STRING)) + bytes(struct.pack('<i',len(obj)*2)) + obj.encode('utf-16le')
    elif isinstance(obj, (bytes)):
        return bytes(struct.pack('<i',PY_CODE_BYTES)) + bytes(struct.pack('<i',len(obj))) + obj
    elif isinstance(obj, (complex)):
        return bytes(struct.pack('<i',PY_CODE_COMPLEX)) + bytes(struct.pack('<d',float(obj.real))) + bytes(struct.pack('<d',float(obj.imag)))
    elif isinstance(obj, (tuple)):
        return pyToBytes(list(obj))
    elif isinstance(obj, (list)):
        count = 0
        res = bytes()
        for i in obj:
            b = pyToBytes(i)
            if len(b) > 0:
                count +=1
                res += b
        return bytes(struct.pack('<i',PY_CODE_LIST)) + bytes(struct.pack('<i',count)) + res
    elif isinstance(obj, (dict)):
        count = 0
        res = bytes()
        for key, value in obj.items():
            b = pyToBytes(value)
            if len(b) > 0:
                res += pyToBytes(key) + b
                count += 1
        return bytes(struct.pack('<i',PY_CODE_DICT)) + bytes(struct.pack('<i',count)) + res 
    elif isinstance(obj, (numpy.ndarray)):
        ar, dt = format_array(obj)
        if ar is None: return None
        res = bytes(struct.pack('<i',PY_CODE_NDARRAY)) + dt.encode() + bytes(struct.pack('<i',len(ar.shape)))
        for s in ar.shape:
            res += bytes(struct.pack('<i',s))
        #if sys.byteorder == 'big':
        #    res += ar.byteswap().tobytes()
        #else:
        #    res += ar.tobytes()
        #return res;
        res += ar.tobytes()
        return res
    elif obj is None:
        return bytes(struct.pack('<i',PY_CODE_NONE))
        
    return bytes()
    
def bytesToPy(b):
    """
    Interpret a serialized object.
    Return a tuple (object, read len)
    """
    if len(b) < 4:
        return (None,None)
        
    dt = struct.unpack('<i',b[0:4])[0]
    
    if dt == PY_CODE_INT: return (struct.unpack('<i',b[4:8])[0], 8)
    elif dt == PY_CODE_LONG: return (struct.unpack('<q',b[4:12])[0], 12)
    elif dt == PY_CODE_DOUBLE: return (struct.unpack('<d',b[4:12])[0], 12)
    elif dt == PY_CODE_COMPLEX: return (complex(struct.unpack('<d',b[4:12])[0], struct.unpack('<d',b[12:20])[0]) , 20)
    elif dt == PY_CODE_STRING: 
        l = struct.unpack('<i',b[4:8])[0]
        obj = b[8:l+8]
        res= (obj.decode('utf-16le') , 8 +l)
        if sys.version_info[0]==2:
            #if __debug_on : __debug("type: " + str(type(obj)) + " " + obj)
            #convert unicode to str
            res= (obj.decode('utf-16le') , 8 +l)
        return res
    elif dt == PY_CODE_BYTES:
        l = struct.unpack('<i',b[4:8])[0]
        return (b[8:l+8] , 8 +l)
    elif dt == PY_CODE_LIST:
        l = struct.unpack('<i',b[4:8])[0]
        start = 8
        res = []
        for i in range(l):
            tmp, size = bytesToPy(b[start:])
            res += [tmp]
            start += size
        return (res,start)
    elif dt == PY_CODE_DICT:
        l = struct.unpack('<i',b[4:8])[0]
        start = 8
        res = {}
        for i in range(l):
            key, size = bytesToPy(b[start:])
            start += size
            tmp, size = bytesToPy(b[start:])
            start += size
            res[key] = tmp
        return (res, start)
        
    elif dt == PY_CODE_POINT_VECTOR:
        res, l = bytesToPy(b[4:])
        return (res,l+4)
    elif dt == PY_CODE_COMPLEX_POINT_VECTOR:
        res, l = bytesToPy(b[4:])
        return (res,l+4)
    elif dt == PY_CODE_INTERVAL_SAMPLE_VECTOR:
        res, l = bytesToPy(b[4:])
        return (res,l+4)
    elif dt == PY_CODE_NDARRAY:
        nt = b[4:5]
        sc = struct.unpack('<i',b[5:9])[0]  
        shape = []
        start = 9
        size=1
        for s in range(sc):
            shape += [struct.unpack('<i',b[start:start+4])[0] ]
            start += 4
            size *= shape[-1]
        nt = numpy.dtype(nt)
        res= numpy.frombuffer(b[start:],nt)
        
        if __debug_on : __debug("read array " + str(len(b)) + " " +str(shape) + " " + str(nt) + " " + str(res.shape))
        
        res = res.reshape(shape).copy()
        #if sys.byteorder == 'big':
        #    res = res.byteswap()
        return (res, start + size * nt.itemsize)
    elif dt == PY_CODE_NONE:
        return (None,4)
        
    return (None,None)


def send_exception():
    """
    Send the last exception to the process output stream, using the strating code 'x'
    """
    exc_type, exc_value, exc_traceback = sys.exc_info()
    l = traceback.format_exception(exc_type, exc_value,exc_traceback)
    res = bytes(struct.pack('<i',PY_CODE_ERROR)) + pyToBytes(''.join(l))
    writeOutput(b'x' + bytes(struct.pack('<i',len(res))) + res)
    
    if __debug_on : __debug("send_exception: " + ''.join(l))
    
def write_chuncked(data):
    
    #sys.__stdout__.buffer.write(data)
    #sys.__stdout__.flush()
    #if __debug_on : __debug("write_chuncked: " + str(len(data)))
    #return None
    chunck_size = 100000
    chuncks = int(len(data)/chunck_size)
    
    m=0
    for i in range(chuncks):
        start = i*chunck_size;
        m += len(data[start:start+chunck_size])
        writeOutput(data[start:start+chunck_size])
    end = chunck_size*chuncks
    if end != len(data):
        m += len(data[end:])
        writeOutput(data[end:])


def send_object(obj):
    """
    Send and object to the process output stream, using the strating code 'b'
    """
    try:
        res = pyToBytes(obj)
        writeOutput(b'b' + bytes(struct.pack('<i',len(res))) + res)
        #sys.__stdout__.buffer.write(b'b' + bytes(struct.pack('<i',len(res))))
        #sys.__stdout__.flush()
        #write_chuncked(res)
        if __debug_on : __debug("send_object " + str(obj) + " done")
    except:
        if __debug_on : __debug("send_object error")
        send_exception()
        
def send_wait_for_input():
    """
    Send and object to the process output stream, using the strating code 'b'
    """
    try:
        if __debug_on : __debug("send_wait_for_input\n")
        writeOutput(b'i')
        if __debug_on : __debug("send_wait_for_input done")
    except:
        if __debug_on : __debug("in send_wait_for_input")
        send_exception()

def write_output(out):
    """
    Send a string to the process output stream, using the starting code 'o'
    """
    if __debug_on : __debug("write_output :"  +"(" +str(type(out)) +") " + out +"\n")
    res = pyToBytes(out)
    writeOutput(b'o' + bytes(struct.pack('<i',len(res))) + res)
    
def write_error(out):
    """
    Send a string to the process output stream, using the starting code 'o'
    """
    if __debug_on : __debug("write_error :" +"(" +str(type(out)) + "," + str(len(out)) +") " + out +"\n")
    res = pyToBytes(out)
    writeOutput(b'e' + bytes(struct.pack('<i',len(res))) + res)
    
def eval_code(code):
    try:
        res = eval(code,globals())
        send_object(res)
    except SyntaxError:
        try:
            exec(code,globals())
            send_object(None)
        except:
            send_exception()
    except:
        send_exception()

def interpret_input():
    """
    Interpret an incomming request starting with a one character code.
    Codes are:
        'q' to exit
        'e' to exec a python code
        's' to send an object based on its name
        'r' to retrieve and object with its name and insert it in the global namespace
        'i' to read a raw input
    """
    code = readInput(1)
    if len(code) == 0:
        return None
    if __debug_on : __debug("received")
    
    if code == b'q':
        #exit
        if __debug_on : __debug("code exit")
        sys.exit(0)
        
    elif code == b'c':
        #eval python code
        l = struct.unpack('<i',readInput(4))[0]
        b = readInput(l)
        py_code, l = bytesToPy(b)
        eval_code(str(py_code))
        
    elif code == b'e':
        if __debug_on : __debug("received exec")
        #exec string code and send 0 on success, exception object on error
        l = struct.unpack('<i',readInput(4))[0]
        if __debug_on : __debug("size: " + str(l) )
        b = readInput(l)
        py_code, l = bytesToPy(b)
        if __debug_on : __debug("code: " + py_code )
        try:
            if __debug_on : __debug("exec '" + py_code +"'")
            exec(str(py_code),globals()) 
            if __debug_on : __debug("done1")
            send_object(None)
            if __debug_on : __debug("done2")
        except SystemExit:
            if __debug_on : __debug("exec SystemError");
            raise
        except:
            if __debug_on : __debug("in code e")
            send_exception()
            
    elif code == b's':
        #send an object
        l = struct.unpack('<i',readInput(4))[0]
        b = readInput(l)
        try:
            name, l = bytesToPy(b)
            value = globals()[str(name)]
            send_object(value)
        except:
            if __debug_on : __debug("in code s\n")
            send_exception()
            
    elif code == b'r':
        #read an object
        #sta = current_milli_time()
        l = struct.unpack('<i',readInput(4))[0]
        b = readInput(l)
        try:
            name, l = bytesToPy(b)
            value, l = bytesToPy(b[l:])
            globals()[str(name)] = value

            if __debug_on : __debug("read object " + str(name) + " of type " + str(type(value)))
            send_object(None)
            #el = current_milli_time() -sta
            #if open("test.txt","a").write(str(el) + "\n"):pass
        except:
            if __debug_on : __debug("in code r")
            send_exception()
            
    elif code == b'i':
        #read and return input
        l = struct.unpack('<i',readInput(4))[0]
        b = readInput(l)
        value, l = bytesToPy(b)
        return value
        
    return None


def format_exec(code):
    c = pyToBytes(code)
    c = b'e' + struct.pack("<i",len(c)) + c
    return c
    
def test_proc():
    import subprocess
    p = subprocess.Popen("python sanstitre1.py",stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.STDOUT)
    return p
    
class RedirectOut:
    """Redirect the standard output"""
    def write(self, chain): write_output(chain)
    def fileno(self): return sys.__stdout__.fileno()   
    def clear(self): pass
    def flush(self): pass

class RedirectErr:
    """Redirect the standard output"""
    def write(self, chain): write_error(chain)
    def fileno(self): return sys.__stdout__.fileno()   
    def clear(self): pass
    def flush(self): pass

class RedirectIn:
    """Redirect the standard input"""
    
    def readline(self):
        send_wait_for_input()
        while globals()["__stop_loop"] == False:
            try:
                line = interpret_input()
                if line is not None:
                    if isinstance(line, (bytes)):
                        line = line.decode()
                    return line
                time.sleep(0.01)
            except SystemExit:
                #if globals()["__debug_on"] : __debug("readline SystemExit")
                globals()["__stop_loop"] = True
                raise
            except:
                #if globals()["__debug_on"] : __debug("readline except")
                exc_type, exc_value, exc_traceback = sys.exc_info()
                l = repr(traceback.format_exception(exc_type, exc_value,exc_traceback))
                #if globals()["__debug_on"] : __debug(''.join(l) )
                pass
                
        
    def fileno(self): return 0       
    def clear(self): pass
    def flush(self): pass
    def tell(self): return 0

def main_loop():
    
    globals()['__debug_on'] = False

    if sys.platform == "win32" and sys.version_info[0]==2:
        #make sure we are in binary mode
        import msvcrt
        msvcrt.setmode(sys.__stdout__.fileno(), os.O_BINARY)
        msvcrt.setmode(sys.__stdin__.fileno(), os.O_BINARY)

    if __debug_on:
        globals()['f'] = open("errors" + str(os.getpid()) +  ".txt","w")
        
    sys.stdin = RedirectIn()
    sys.stdout = RedirectOut()
    sys.stderr = RedirectErr()
    
    if __debug_on : __debug("start main loop")
    
    globals()["__stop_loop"] = False
    while globals()["__stop_loop"] == False:
        try:
            interpret_input()
            time.sleep(0.01)
        except SystemExit:
            if __debug_on : __debug("SystemExit")
            globals()["__stop_loop"] = True
            raise
        except:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            l = traceback.format_exception(exc_type, exc_value,exc_traceback)
            if __debug_on : __debug(''.join(l) )
            pass
        
            

if __name__ == "__main__":
    
    main_loop()
    if __debug_on : __debug("exit")
        
#b = pyToBytes(2)       
#o = bytesToPy(b)
#print(len(b),o)   
#
#b = pyToBytes(2.4)       
#o = bytesToPy(b)
#print(len(b),o)  
#
#b = pyToBytes(2+4.3j)       
#o = bytesToPy(b)
#print(len(b),o)    
#
#b = pyToBytes('toto')       
#o = bytesToPy(b)
#print(len(b),o)  
#
#b = pyToBytes(b'toto')       
#o = bytesToPy(b)
#print(len(b),o)  
#
#b = pyToBytes([2,3.4])       
#o = bytesToPy(b)
#print(len(b),o)  
#
#b = pyToBytes((2,3.4))       
#o = bytesToPy(b)
#print(len(b),o)  
#
#b = pyToBytes({'rr':3.4,'tt':'toto','yy':b'toto'})       
#o = bytesToPy(b)
#print(len(b),o)  
#
#b = pyToBytes(np.zeros((2,3),dtype='d'))       
#o = bytesToPy(b)
#print(len(b),o)  
#
#
#try:
#    raise NameError('HiThere')
#except:
#    send_exception()
#    
