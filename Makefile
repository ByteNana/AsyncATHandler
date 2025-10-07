include makefiles/variables.mk
include makefiles/development.mk
include makefiles/test.mk

.PHONY: all help

all: build

help:
	@echo "Available targets:"
	@awk '{ prev = curr; curr = $$0 }  /^[a-zA-Z0-9_-]+:/ { \
			target = $$1; sub(":", "", target); \
			if (prev ~ /^##[ \t]*/) { \
				comment = prev; sub(/^##[ \t]*/, "", comment); \
				printf "\033[36m%-20s\033[0m %s\n", target, comment; \
			} \
		} \
	' $(MAKEFILE_LIST)

%:
	@:
