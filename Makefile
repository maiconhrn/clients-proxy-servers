all: client proxy server
client: src/client.c
	gcc -o build/client src/client.c

proxy: src/proxy.c
	gcc -o build/proxy src/proxy.c -lpthread

server: src/server.c
	gcc -o build/server src/server.c -lpthread

run_client: build/client
	build/client

run_proxy: build/proxy
	build/proxy

run_server: build/server
	build/server