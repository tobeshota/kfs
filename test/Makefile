include ../.env
export

ISA				?= i386
CC				?= gcc
# -freestanding: 自分たちが定義した標準ライブラリを使用したいため(memcpyとか)
CFLAGS			= -I../include -I./unit -Wall -Wextra -O0 -g -m32 --coverage -DWAITING_FOR_INPUT -ffreestanding
DEPFLAGS		= -MMD -MP -MF $(@:.o=.d)
DOCKER_RUN		= docker run --platform linux/386 -v $(realpath ../):/work -w /work/test unit_test_image
DOCKER_SH		= $(DOCKER_RUN) bash -euxo pipefail -c
LCOV			= lcov --rc lcov_branch_coverage=1
GENHTML			= genhtml --rc lcov_branch_coverage=1
# 単体テスト対象ソース: テストコード + テストしたいカーネルコード最小限
TEST_SRCS		= $(shell find ../test/unit -name '*.c')
# arch/を単体テスト対象ソースから除外した理由：arch/はアーキテクチャ固有の処理であり，単体テストの実行環境(ホスト環境)では必ずしも実行できないため．
KERNEL_SRCS		= $(shell find ../ -path ../test -prune -o -path ../arch -prune -o -name '*.c' -print)
UNIT_SRCS		= $(TEST_SRCS) $(KERNEL_SRCS)

# Build & coverage directories
BUILD_DIR		= build/obj
COVERAGE_DIR	= _artifacts/coverage

# Normalize object paths: ../foo/bar.c -> build/obj/foo/bar.o
UNIT_OBJS		= $(patsubst ../%, $(BUILD_DIR)/%, $(UNIT_SRCS:.c=.o))
UNIT_DEPS		= $(UNIT_OBJS:.o=.d)
UNIT_BIN		= ./unit_test.bin

# Coverage artifacts (searched only under BUILD_DIR)
COVERAGE_FILES	= $(shell find $(BUILD_DIR) -name '*.gcno' -o -name '*.gcda' 2>/dev/null)

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
		$(LCOV) --capture --directory $(BUILD_DIR) --output-file $(COVERAGE_DIR)/coverage.info; \
		$(LCOV) --remove $(COVERAGE_DIR)/coverage.info '/work/test/*' '/usr/*' '*.h' --output-file $(COVERAGE_DIR)/coverage.filtered.info; \
		$(GENHTML) $(COVERAGE_DIR)/coverage.filtered.info --output-directory $(COVERAGE_DIR)/html; \
		$(LCOV) --summary $(COVERAGE_DIR)/coverage.filtered.info"
	@ echo "\nCoverage HTML:\n./test/$(COVERAGE_DIR)/html/index.html\n"

integration:
	@echo "integration test" && \
	bash -eu ./integration/integration_test.sh

clean:
	rm -rf $(BUILD_DIR) $(UNIT_BIN) $(COVERAGE_DIR)

$(UNIT_BIN): $(UNIT_OBJS)
	$(CC) $(CFLAGS) -o $@ $(UNIT_OBJS)


# Pattern rule for any ../foo/bar.c mapped into build/obj/foo/bar.o
$(BUILD_DIR)/%.o: ../%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

-include $(UNIT_DEPS)

.PHONY: all test docker-image unit coverage integration clean
