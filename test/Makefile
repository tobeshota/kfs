LCOV			= lcov --rc lcov_branch_coverage=1
GENHTML			= genhtml --rc lcov_branch_coverage=1
COVERAGE_DIR	= _artifacts/coverage
COVERAGE_FILES	= $(shell find $(BUILD_DIR) -name '*.gcno' -o -name '*.gcda' 2>/dev/null)

all: test

test: unit integration

unit:
	@ make unit -C unit/

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
	@ make clean -C unit/

fclean:
	@ make fclean -C unit/

	rm -rf $(BUILD_DIR) $(UNIT_BIN) $(COVERAGE_DIR)

.PHONY: all test unit coverage integration clean fclean
