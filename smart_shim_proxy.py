#!/usr/bin/env python

# Import all the repy functionalities.

from posix_call_definition import *

from repyportability import *
_context = locals()
add_dy_support(_context)

dy_import_module_symbols("random")
dy_import_module_symbols("struct")


proxy_ip = "127.0.0.1"
proxy_port = 53678


# This is the dictionary that holds the function name
# to the repy function that needs to be called. These
# functions are defined in the posix_call_definition.repy
# library file.
libc_function_dict = { "socket" : call_socket,
                       "bind" : call_bind,
                       "accept" : call_accept,
                       "connect" : call_connect,
                       "listen" : call_listen,
                       "close" : call_close,
                       "shutdown" : call_shutdown,
                       "setsockopt" : call_setsockopt,
                       "getsockopt" : call_getsockopt,
                       "getpeername" : call_getpeername,
                       "getsockname" : call_getsockname,
                       "send" : call_send,
                       "sendto" : call_sendto,
                       "write" : call_write,
                       "recv" : call_recv,
                       "recvfrom" : call_recvfrom,
                       "read" : call_read,
                       "ioctl" : call_ioctl,
                       "fcntl" : call_fcntl
                  }



# =================== Server Functionalities ============================


def master_server():
  """
  <Purpose>
    The master server is the main entry point for all initial
    network activity.
  """

  tcpserversock = listenforconnection(proxy_ip, proxy_port)
  print "[ShimProxy] Starting Master Server on %s:%d" % (proxy_ip, proxy_port)
  print ''

  while True:
    # Receive a new connection.
    remote_ip, remote_port, mastersock = block_call(tcpserversock.getconnection)  

    # Once a new connection is made, launch a new thread to handle the connection.
    print "[ShimProxy] Received connection from %s:%d" % (remote_ip, remote_port)
    print ''
    createthread(handle_new_sock_connection(mastersock))





def generate_new_id():
  """
  <Purpose>
    This function is used to generate a new unique identifier for a
    new connection.
  """
  return 10000 * randomfloat()



def handle_new_sock_connection(mastersock):
  """
  <Purpose>
    Once a connection is made, this thread is responsible
    for this particular connection. Here we constantly keep
    listening and perform the requested action by the application.

  <Arguments>
    mastersock - the socket connection for an open socket to
      the application.

  <Exceptions>
    None

  <Side Effects>
    An extra socket is opened up if connect or listen is called.

  <Return>
    The function _handle_new_connection_helper.
  """

  def _handle_new_connection_helper():

    # Generate a new connection id for this connection.
    # connection_id = int(generate_new_id())

    # We will keep track of what socket this is.
    thissockfd = None

    while True:
      try:
        msg_recv = block_call(mastersock.recv, 4096)

        arg_len = len(msg_recv) - 21
        # Unpack the received message into the function call and arg list.
        list_recv = struct_unpack("19s%ds" % arg_len, msg_recv)

        # Since the data sent from the C side will have lots
        # of garbage (mostly null characters) after the actual data,
        # we clean up unpacked data.
        call_func = list_recv[0].strip('\0')
        call_args = list_recv[1].strip('\0')
      
        print "[NetRecv] Call '%s' for sock '%s' with args '%s'" % (call_func, str(thissockfd), call_args)

        # Check that if it is a legal Posix call. If it is then we call the 
        # appropriate function to handle it.
        if call_func not in libc_function_dict.keys():
          raise PosixCallNotFound("The call '%s' could not be recognized." % call_func)
        
        # Call the libc function with the unique connection id and arguments
        # provided for this call.
        (return_val, err_val) = libc_function_dict[call_func](call_args)
        print "Return Val for %s is '%s' and err: '%s'" % (call_func, str(return_val), str(err_val))

        # If this is creating a new socket, then we keep track of the
        # socket fd.
        if call_func == 'socket':
          thissockfd = return_val

        # Pack up the message and send it back to the C server.
        call_return_tuple = (err_val, return_val)
        struct_format = "<i%ds" % len(return_val)
        packed_msg = struct_pack(struct_format, err_val, return_val)        

        print "[NetSend] Return result for call '%s' for sock '%s': '%s':%d" % (call_func, str(thissockfd), return_val, err_val)
        print ''

        block_call(mastersock.send, packed_msg)
      except (SocketClosedRemote, SocketClosedLocal), err:
        print "[ShimProxy] Socket closed detected for sock '%s'." % str(thissockfd)
        print ''

        # Check to see if we know the fd of this socket.
        if thissockfd:
          # We are going to call shutdown on the socket regardless
          # whether it was closed already.
          try:
            call_close(thissockfd)
          except:
            pass
        break
      except Exception, err:
        raise
        print "[ShimProxy] Error handling call: '%s'" % str(err)

  return _handle_new_connection_helper




# ========================== Assorted Functions ================================================








# ========================== Define Exceptions =================================================

class PosixCallNotFound(Exception):
  """ 
  Error raised if an unseen Posix call is found that we don't
  have the equivalent Repy call for.
  """
  pass

# ==============================================================================================


def main():
  """
  <Purpose>
    This is the main launching point for the smart server.
    We launch the master server that handles all socket 
    network activity.
  """
  master_server()


if __name__ == '__main__':
  main()
