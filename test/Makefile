include ../.env
export

ISA				?= i386
CC				?= gcc
CFLAGS			= -I../include -I./unit -Wall -Wextra -O0 -g -m32 -fprofile-arcs -ftest-coverage
DOCKER_RUN		= docker run --platform linux/386 -v $(realpath ../):/work -w /work/test unit_test_image
DOCKER_SH		= $(DOCKER_RUN) bash -euxo pipefail -c
LCOV			= lcov --rc lcov_branch_coverage=1
GENHTML			= genhtml --rc lcov_branch_coverage=1
UNIT_SRCS		= $(shell find ../ -name '*.c')
COVERAGE_FILES	= $(shell find ../ -name '*.gcno' -o -name '*.gcda')
UNIT_OBJS		= $(UNIT_SRCS:.c=.o)
UNIT_BIN		= ./unit_test.bin
COVERAGE_DIR	= _artifacts/coverage

all: test

test: unit integration

docker-image:
	docker build --platform linux/386 -f ./unit/arch/$(ISA)/compile.dockerfile -t unit_test_image ../

unit: docker-image
	$(DOCKER_SH) "make $(UNIT_BIN) && $(UNIT_BIN)"

# $(COVERAGE_DIR)を都度再生成している理由は，カバレッジの測定が以前の結果に影響されないようにするため
coverage: unit
	@ $(DOCKER_SH) "\
		rm -rf $(COVERAGE_DIR) && mkdir -p $(COVERAGE_DIR)/html; \
		$(LCOV) --capture --directory /work --output-file $(COVERAGE_DIR)/coverage.info; \
		$(LCOV) --remove $(COVERAGE_DIR)/coverage.info '/work/test/*' '/usr/*' --output-file $(COVERAGE_DIR)/coverage.filtered.info; \
		$(GENHTML) $(COVERAGE_DIR)/coverage.filtered.info --output-directory $(COVERAGE_DIR)/html"
	@ echo "\nCoverage HTML:\n./test/$(COVERAGE_DIR)/html/index.html\n"

integration:
	@echo "integration test" && \
	bash -eu ./integration/integration_test.sh

clean:
	rm -rf build $(UNIT_OBJS) $(COVERAGE_FILES)

$(UNIT_BIN): $(UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(UNIT_OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all test docker-image unit coverage integration clean
