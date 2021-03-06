#!python
import time
import socket
import traceback

from threading import Thread

host = '127.0.0.1'
port = 23452

# This is the server thread that will wait for a connection.
class server(Thread):
  def __init__(self, host, port):
    Thread.__init__(self)
    self.host = host
    self.port = port

  def run(self):
    sock_server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    print "Created server socket"
    sock_server.bind((self.host, self.port))
    sock_server.listen(1)

    conn, addr = sock_server.accept()   

    # This should block and not raise any error.
    try:
      data = conn.recv(1024)
      print "[Server] Received data: " + data
    except Exception, err:
      return
    


# Start the server then wait couple of seconds 
# before sending it message.
server_thread = server(host, port)
server_thread.start()
time.sleep(3)

# Open up a connection to the server and send a short message.
sock_client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
print "Created client socket"
try:
  sock_client.connect((host, port))
  
  print "[Client] Sending data: Hello World"
  sock_client.send('Hello World')
except:
  print traceback.format_exc()
  raise
sock_client.close()
