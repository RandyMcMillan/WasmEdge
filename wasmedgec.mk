



wasmedge:## 	wasmedge
	@rm -rf /usr/local.bin/wasmedge
	curl -sSf https://raw.githubusercontent.com/WasmEdge/WasmEdge/master/utils/install.sh | bash -s -- -e all -p /usr/local/bin/wasmedge
	type -P wasmedgec

wasmedge-docker:
	@docker pull wasmedge/slim:0.11.2
wasmedge-docker-hello:
	@docker run -it --rm -v $(PWD):/app wasmedge/slim:0.11.2 wasmedge hello.aot.wasm world hello
