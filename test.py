import socket
import time

HOST = "127.0.0.1"
PORT = 8080

request_parts = [
    b"GET / HTTP/1.1\r\n",
    b"Host: localhost\r\n",
    b"Connection: close\r\n",
    b"\r\n"
]

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.connect((HOST, PORT))

for i, part in enumerate(request_parts):
    sock.sendall(part)
    print("Enviado bloco", i + 1, ":", repr(part))
    time.sleep(0.5)

response = b""

while True:
    data = sock.recv(4096)

    if not data:
        break

    response += data

print("\nResposta recebida:")
print(response.decode(errors="replace"))

sock.close()