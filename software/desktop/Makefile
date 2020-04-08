all:
	pip install -r requirements.txt
	@echo -ne "Executing command: "
	pyinstaller --clean --onefile main.pyw
	@mv dist/main NotKaimana
	@echo "Cleaning up..."
	@rm -rf build dist main.spec
