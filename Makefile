# Native build:      make                    (default preset: arch-debug)
#                    make PRESET=arch-release
# Containerised OS:  make docker-debian      (needs docker/debian.Dockerfile
#                                             and a debian-release preset)
#                    Output lands in build/<os>-release/
PRESET ?= arch-debug

.PHONY: build docker-%

build:
	cmake --preset $(PRESET)
	cmake --build --preset $(PRESET)

docker-%:
	docker build -f docker/$*.Dockerfile --target export \
	    --output type=local,dest=build/$*-release .