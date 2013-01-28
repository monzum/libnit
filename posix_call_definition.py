#!python
"""
<Library Module>
  posix_call_definition.repy

<Author>
  Monzur Muhammad
  monzum@cs.washington.edu

<Date>
  March 30th, 2012

<Purpose>
  This library defines the various POSIX calls
  that translates to Repy calls. This module is
  meant to be imported.
"""

# Import all the repy functionalities and lind
# functionalities.
from repyportability import *
#from lind_fs_calls import *
#from lind_net_calls import *
from lind_fs_constants import *
from lind_net_constants import *

execfile('lind_fs_calls.py')
execfile('lind_net_calls.py')


_context = locals()
add_dy_support(_context)

# Import the serialize library.
dy_import_module_symbols("serialize")

# Import the shim library.
dy_import_module_symbols("shimstackinterface")



# Define some shim/repy related variables.
DEFAULT_TIMEOUT = 10

# By default use the NoopShim.
#shim_string = "(LogShim,smart_shim_log.txt)"
shim_string = "(CompressionShim)"

shim_obj = ShimStackInterface(shim_string)

listenforconnection = shim_obj.listenforconnection
openconnection = shim_obj.openconnection






# The list of error values that can be returned.
# These definitions were found on linux in the file:
# /usr/src/linux-headers-3.0.0-12/include/asm-generic/errno-bash.h and
# /usr/src/linux-headers-3.0.0-12/include/asm-generic/errno.h
error_dict = {"EPERM" : 1,        # Operation not permitted.
              "ENOENT" : 2,       # No such file or directory.
              "EIO" : 5,          # I/O error.
              "ENXIO" : 6,        # No such device or address.
              "EBADF" : 9,        # Bad file number.
              "EAGAIN" : 11,      # Try again.
              "EFAULT" : 14,      # Bad address.
              "ENOTBLK" : 15,     # Block device required.
              "EBUSY" : 16,       # Device or resource busy.
              "EINVAL" : 22,      # Invalid Argument.
              "EWOULDBLOCK" : 22, # Operation would block.
              "EMFIL" : 24,       # Too many open files.
              "ENOMSG" : 42,      # No message of desired type.
              "ECOMM" : 70,       # Communication error on send.
              "EPROTO" : 71,      # Protocol error.
              "EBADMSG" : 74,     # Not a data message.
              "EREMCHG" : 78,     # Remote address changed.
              "ENOTSOCK" : 88,    # Socket operation on non-socket.
              "EDESTADDRREQ" : 89, # Destination address required.
              "EMSGSIZE" : 90,    # Message too lon.
              "EPROTOTYPE" : 91,  # Protocol wrong type for socket.
              "EPROTONOSUPPORT" : 93, # Protocol not supported.
              "ESOCKTNOSUPPORT" : 94, # Socket type not supported.
              "EOPNOTSUPP" : 95,  # Operation not supported on transport endpoint.
              "EPFNOSUPPORT" : 96, # Protocol family not supported.
              "EAFNOSUPPORT" : 97, # Addresss family not supported by protocol.
              "EADDRINUSE" : 98,  # Address already in use.
              "EADDRNOTAVAIL" : 99, # Cannot assign requested address.
              "ENETDOWN" : 100,   # Network is down.
              "ENETUNREACH" : 101, # Network is unreachable.
              "ENETRESET" : 102,  # Network dropped connection because of reset.
              "ECONNRESET" : 104, # Connection reset by peer.
              "ENOBUFS" : 105,    # No buffer space available.
              "EISCONN" : 106,    # Transport endpoint is already conencted.
              "ENOTCONN" : 107,   # Transport endpoint is not connected.
              "ESHUTDOWN" : 108,  # Cannot send after transport endpoint shutdown.
              "ETIMEDOUT" : 110,  # Connection timed out.
              "ECONNREFUSED" : 111, #  Connection refused.
              "EHOSTDOWN": 112,   # Host is down.
              "EHOSTUNREACH" : 113 # No route to host.
              }


# Define various globals.
AF_INET = 2
SOCK_STREAM = 1
SOCK_DGRAM = 2

conn_family = [SOCK_STREAM, SOCK_DGRAM]






# ========================== Common Calls =====================================================

def block_call(call_func, *args):
  """
  <Purpose>
    Make a blocking call for the function.
  
  <Exception>
    None

  <Return>
    Return whatever value the call_func returns.
  """

  while True:
    try:
      return call_func(*args)
    except SocketWouldBlockError:
      sleep(0.01)



def get_available_port(conn_type):
  """
  Find a free port that is available and return it.
  """
  
  resource_type = ''

  if conn_type == 'tcp':
    resource_type = 'connport'
  elif conn_type == 'udp':
    resource_type = 'messport'
  else:
    raise Exception("Conn type must be udp or tcp")

  # We look up the allowed ports and which ports are already being used.
  # Then we pick an available port and return it.
  (resource_list_limit, resource_list_usage, stoptimes) = getresources()
  available_ports = list(resource_list_limit[resource_type] - resource_list_usage[resource_type])

  # Choose a random port out of the available ports.
  rand_index = int(randomfloat() * (len(available_ports)-1))

  if rand_index < 0:
    raise Exception("No localports available")

  try:
    port = int(available_ports[rand_index])
  except:
    log("index is: " + str(rand_index) + " Available_ports: " + str(available_ports))
    raise

  return port



# ========================== Define Posix Calls to Their Repy Alternative ======================

def call_socket(arg_list_str):
  # Split the arguments and build the connection dict.
  try:
    domain_str, conn_type_str, protocol_str = arg_list_str.split(',')
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  domain = int(domain_str)
  conn_type = int(conn_type_str)
  protocol = int(protocol_str)

  try:
    new_sock = socket_syscall(domain, conn_type, protocol)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])
  
  # Everything is done, so we return with no error.
  return (str(new_sock), -1)






def call_bind(arg_list_str):
  # Split the arguments necessary for bind.
  try:
    fd_str, ip_addr, port_str = arg_list_str.split(',') 
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  fd = int(fd_str)
  port = int(port_str)

  # Call the bind call from lind.
  try:
    return_val = bind_syscall(fd, ip_addr, port)
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])
            
  # We are done with the binding.
  return (str(return_val), -1)






def call_accept(arg_list_str):
  # Split the arguments necessary for bind.
  try:
    fd = int(arg_list_str) 
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  # Call the accept call from lind.
  try:
    remoteip, remoteport, newsock_fd = accept_syscall(fd)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  # The return value will be in the form: "remoteip,remoteport,newsock_fd"
  return_val = ','.join([remoteip, str(remoteport), str(newsock_fd)])

  return (return_val, -1)

  




def call_connect(arg_list_str):
  # Split the arguments necessary for bind.
  try:
    fd_str, destip, port_str = arg_list_str.split(',') 
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  fd = int(fd_str)
  destport = int(port_str)

  # Call the connect call from lind.
  try:
    return_val = connect_syscall(fd, destip, destport)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)






def call_listen(arg_list_str):
  # Get the backlog, but we don't do anything with it.
  try:
    fd_str, backlog_str = arg_list_str.split(',')
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  fd = int(fd_str)
  backlog = int(backlog_str)

  # Call the listen call from lind.
  try:
    return_val = listen_syscall(fd, backlog)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)
   




def call_select(arg_list_str):
  # Parse the arguments and process the select call.
  pass



def call_shutdown(arg_list_str):
  # Parse the arguments and get the fd and how.
  try:
    fd_str, how_str = arg_list_str.split(',')
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  fd = int(fd_str)
  how = int(how_str)

  # Call the listen call from lind.
  try:
    return_val = setshutdown_syscall(fd, how)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)





def call_close(arg_list_str):
  # Parse the arguments and get the fd and how.
  try:
    fd = int(arg_list_str)
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])


  # Call the listen call from lind.
  try:
    return_val = close_syscall(fd)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)






def call_setsockopt(arg_list_str):
  # Parse the arguments and get the fd and how.
  try:
    fd_str, level_str, optname_str, optval_str = arg_list_str.split(',')
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  fd = int(fd_str)
  level = int(level_str)
  optname = int(optname_str)
  optval = int(optval_str)

  # Call the listen call from lind.
  try:
    return_val = setsockopt_syscall(fd, level, optname, optval)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)






def call_getsockopt(arg_list_str):
  # Parse the arguments and get the fd and how.
  try:
    fd_str, level_str, optname_str = arg_list_str.split(',')
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  fd = int(fd_str)
  level = int(level_str)
  optname = int(optname_str)

  # Call the listen call from lind.
  try:
    return_val = getsockopt_syscall(fd, level, optname)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)





def call_getpeername(connection_id, arg_list_str):
  # Parse the arguments and get the fd and how.
  try:
    fd = int(arg_list_str)
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  # Call the listen call from lind.
  try:
    remoteip, remoteport = getpeername_syscall(fd)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  # The return value will be in the form of: "remoteip, remoteport, int_value"
  return_val = ','.join([remoteip, str(remoteport), '0'])

  return (str(return_val), -1)
  




def call_getsockname(connection_id, arg_list_str):
  # Parse the arguments and get the fd and how.
  try:
    fd = int(arg_list_str)
  except:
    # If we can't split the argument properly.
    return ('', error_dict["EINVAL"])

  # Call the listen call from lind.
  try:
    localip, localport = getsockname_syscall(fd)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  # The return value will be in the form of: "localip, localport, int_value"
  return_val = ','.join([localip, str(localport), '0'])

  return (str(return_val), -1)
  pass






def call_send(arg_list_str):
  # Split the arguments.
  # Split only twice as the message may contain ','
  fd_str, flags_str, msg = arg_list_str.split(',', 2)

  fd = int(fd_str)
  flags = int(flags_str)
  
  # Call the send call from lind.
  try:
    return_val = send_syscall(fd, msg, flags)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)
  
  
    
    


def call_sendto(connection_id, arg_list_str):
  # Split the arguments.
  # Split it only 4 times as the message itself may contain ','.
  fd_str, flags_str, remoteip, remoteport_str, msg = arg_list_str.split(',', 4)

  fd = int(fd_str)
  flags = int(flags_str)
  remoteport = int(remoteport_str)
  
  # Call the send call from lind.
  try:
    return_val = sendto_syscall(fd, msg, remoteip, remoteport, flags)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)




def call_write(arg_list_str):
  # Split the arguments.
  # Split only twice as the message may contain ','
  fd_str, msg = arg_list_str.split(',', 1)

  fd = int(fd_str)
  
  # Call the write call from lind.
  try:
    return_val = send_syscall(fd, msg,0)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (str(return_val), -1)




def call_recv(arg_list_str):
  # Get the argument of recv length out.
  fd_str, recv_size_str, flags_str = arg_list_str.split(',')

  fd = int(fd_str)
  recv_size = int(recv_size_str)
  flags = int(flags_str)

  # Call the send call from lind.
  try:
    return_msg = recv_syscall(fd, recv_size, flags)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (return_msg, -1)
  



def call_recvfrom(connection_id, arg_list_str):
  # Get the argument of recv length out.
  fd_str, recv_size_str, flags_str = arg_list_str.split(',')

  fd = int(fd_str)
  recv_size = int(recv_size_str)
  flags = int(flags_str)

  # Call the send call from lind.
  try:
    remoteip, remoteport, return_msg = recvfrom_syscall(fd, recv_size, flags)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  # The return value will be of the form: "remoteip,remoteport,actual_return_msg".
  return_val = ','.join([remoteip, remoteport, return_msg])

  return (return_val, -1)





def call_read(arg_list_str):
  # Get the argument of recv length out.
  fd_str, recv_size_str = arg_list_str.split(',')

  fd = int(fd_str)
  recv_size = int(recv_size_str)

  # Call the read call from lind.
  try:
    return_msg = recv_syscall(fd, recv_size, 0)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (return_msg, -1)





def call_ioctl(connection_id, arg_list_str):
  pass



def call_fcntl(connection_id, arg_list_str):
  # Get the argument of recv length out.
  full_argument_list = arg_list_str.split(',')

  fd = int(full_argument_listt[0])
  cmd = int(full_argument_list[1])

  cmd_list_int = []
  
  # Convert all the command arguments to integer.
  for cur_arg in full_argument_list[2:]:
    cmd_list_int.append(int(cur_arg))

  # Turn the list into a tuple so we can send it to lind
  # as variable arguments.
  cmd_arg_tuple = tuple(cmd_list_int)


  # Call the read call from lind.
  try:
    return_msg = fcntl_syscall(fd, cmd, *cmd_arg_tuple)
  except UnimplementedError:
    return ('', error_dict["EPROTONOSUPPORT"])
  except SyscallError, (err_call, err_name, err_msg):
    return ('', error_dict[err_name])

  return (return_msg, -1)
  
  



# ========================== End Posic Calls Definition ========================================
