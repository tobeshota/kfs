# 含まれるルール：
# ・単体テスト
# ・結合テスト

test: unit integration

unit:
	@echo "unit test"

integration:
	@echo "integration test" && \
	bash -eu ./integration_test.sh

.PHONY: test unit integration
