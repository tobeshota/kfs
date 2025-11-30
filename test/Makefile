all: test

test: unit integration

unit:
	@ make unit -C unit/

coverage:
	@ make coverage -C unit/

integration:
	@echo "integration test" && \
	bash -eu ./integration/integration_test.sh

clean:
	@ make clean -C unit/

fclean:
	@ make fclean -C unit/

	rm -rf $(BUILD_DIR) $(UNIT_BIN) $(COVERAGE_DIR)

.PHONY: all test unit coverage integration clean fclean
