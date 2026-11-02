# talon: the LithTech "Talon" tools (AvP2 Editing Tools, Update 2) and the
# NOLF and AvP2 world processors, rebuilt from source.
#
#   make            build every reconstructed tool (build/build.mk)
#   make reference  extract the shipped binaries and their function tables
#                   into out/reference/ (build/reference.sh)
#   make progress   score the build against the shipped binaries
#   make test       run the shipped tools and ours on the same inputs and
#                   compare what they write (tests/README.md)
#   make shell      a shell in the build container
#   make help       everything else
#
# Needs docker and the three submodules (`git submodule update --init`):
# toolchains/ (the compilers: toolchains/README.md; this project uses
# msvc-6.00-8966 and msvc-6.00-9044, and for NOLF's processor
# msvc-6.00-8447 and icl-4.0), rez/ (its disc-image readers) and
# tools/ (the reconstruction tools). Every step runs in a throwaway
# container of the cjacksrc build image (Ubuntu 24.04 with 32-bit Wine),
# with this directory mounted at /repo, as you.

IMAGE ?= cjacksrc-build
JOBS  ?= $(shell nproc)

DOCKER_RUN = docker run --rm --init -u $(shell id -u):$(shell id -g) \
	-e HOME=/tmp/home -v $(CURDIR):/repo -w /repo $(IMAGE)

TOOLCHAIN_FILES = toolchains/msvc-6.00-8966/BIN/cl.exe toolchains/msvc-6.00-9044/BIN/cl.exe \
	toolchains/msvc-6.00-8447/BIN/cl.exe toolchains/icl-4.0/BIN/icl.exe

.PHONY: all reference progress progress-update test shell clean help check-toolchain

all: check-toolchain
	$(DOCKER_RUN) make -f build/build.mk -j$(JOBS)

help:
	@echo "make            build every reconstructed tool"
	@echo "make reference  extract the shipped binaries into out/reference/"
	@echo "make progress   score the build against them (COMPLETE=1, COVERED=1)"
	@echo "make progress-update  record the score in progress/ (ACCEPT_LOSSES=1)"
	@echo "make test       behaviour tests against the shipped tools (TESTS=suite...)"
	@echo "make shell      a shell in the build container"
	@echo "make clean      remove out/ and every build output in src/"

check-toolchain:
	@for f in $(TOOLCHAIN_FILES); do [ -f $$f ] || { \
	  echo "missing $$f: build the compilers first (toolchains/README.md:"; \
	  echo "  bash toolchains/fetch.sh msvc-6.00-8966 msvc-6.00-9044 msvc-6.00-8447 icl-4.0, in the build container)"; exit 1; }; done

reference:
	$(DOCKER_RUN) bash build/reference.sh

# what tools/evidence.py cannot ask git for in the container (talon is a
# directory of lithtools, whose .git is not mounted)
EVIDENCE_ENV = EVIDENCE_REVISION=$(shell git rev-parse HEAD 2>/dev/null) \
	EVIDENCE_MODIFIED=$(shell git status --porcelain --untracked-files=no . 2>/dev/null | wc -l) \
	EVIDENCE_SUBMODULES="$(shell git submodule status 2>/dev/null | sed 's/^ *//' | tr '\n' ';')"

progress:
	$(DOCKER_RUN) env COMPLETE=$(COMPLETE) COVERED=$(COVERED) $(EVIDENCE_ENV) bash build/progress.sh check

progress-update:
	$(DOCKER_RUN) env ACCEPT_LOSSES=$(ACCEPT_LOSSES) $(EVIDENCE_ENV) bash build/progress.sh update

test:
	$(DOCKER_RUN) bash tests/run.sh $(TESTS)

shell:
	docker run --rm --init -it -u $(shell id -u):$(shell id -g) \
	  -e HOME=/tmp/home -v $(CURDIR):/repo -w /repo $(IMAGE) bash

clean:
	rm -rf out .build .wine
	find src -depth -type d -iname built -exec rm -rf {} +
