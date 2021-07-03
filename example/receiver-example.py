import socket
import time
import msgpack
import magic
import threading

class ClientThread(threading.Thread):
    def __init__(self, socket):
        self.socket = socket
        super().__init__()

    def run(self, *args, **kwargs):
        unpacker = msgpack.Unpacker(raw=True)
        while True:
            chunk = self.socket.recv(4096)
            if chunk == b'':
                print('Client disconnected')
                break
            unpacker.feed(chunk)
            for unpacked in unpacker:
                img, meta = unpacked
                print('Matches -> ', meta)
                # print(magic.from_buffer(img))

serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
serversocket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
serversocket.bind(("", 7071))
serversocket.listen(5)
while True:
    # accept connections from outside
    (clientsocket, address) = serversocket.accept()
    print('Client connected')
    # now do something with the clientsocket
    # in this case, we'll pretend this is a threaded server
    ct = ClientThread(clientsocket)
    ct.start()