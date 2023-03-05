cargo-build:## 	cargo-build
	@. $(HOME)/.cargo/env
	@echo cargo b
	@cargo b
cargo-install:## 	cargo-install
	@. $(HOME)/.cargo/env
	@echo cargo install --path $(PWD)
	@cargo install --path $(PWD)
	@echo "export PATH=$(CARGO_PATH)/bin:$(PATH)"
cargo-build-release:## 	cargo-build-release
	@. $(HOME)/.cargo/env
	@echo cargo b --release
	@cargo b --release
cargo-check:## 	cargo-check
	@. $(HOME)/.cargo/env
	@echo cargo c
	@cargo c
cargo-bench:## 	cargo-bench
	@. $(HOME)/.cargo/env
	@echo cargo bench
	@cargo bench
cargo-clean:## 	cargo-clean
	@. $(HOME)/.cargo/env
	@echo cargo clean
	@cargo clean
cargo-test:## 	cargo-test
	@. $(HOME)/.cargo/env
	@echo cargo test
	@cargo test
# vim: set noexpandtab:
# vim: set setfiletype make
