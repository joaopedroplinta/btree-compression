CXX      = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra

ZIP_NAME = jpshp

.PHONY: all clean zip

all: btree compress

btree: btree.cpp
	$(CXX) $(CXXFLAGS) -o btree btree.cpp

compress: compress.cpp
	$(CXX) $(CXXFLAGS) -o compress compress.cpp

clean:
	rm -f btree compress *.bin *.lzw *.huf
	rm -f relatorio.aux relatorio.log relatorio.toc relatorio.out

zip: relatorio.pdf
	@echo "[zip] Gerando $(ZIP_NAME).zip..."
	@PARENT=$$(cd .. && pwd); \
	 rm -rf $$PARENT/$(ZIP_NAME); \
	 mkdir -p $$PARENT/$(ZIP_NAME); \
	 cp -r . $$PARENT/$(ZIP_NAME)/; \
	 rm -rf $$PARENT/$(ZIP_NAME)/.git \
	        $$PARENT/$(ZIP_NAME)/.github \
	        $$PARENT/$(ZIP_NAME)/CLAUDE.md; \
	 rm -f  $$PARENT/$(ZIP_NAME)/btree \
	        $$PARENT/$(ZIP_NAME)/compress \
	        $$PARENT/$(ZIP_NAME)/relatorio.tex \
	        $$PARENT/$(ZIP_NAME)/ifpr-pinhais.cls \
	        $$PARENT/$(ZIP_NAME)/relatorio.aux \
	        $$PARENT/$(ZIP_NAME)/relatorio.log \
	        $$PARENT/$(ZIP_NAME)/relatorio.toc \
	        $$PARENT/$(ZIP_NAME)/relatorio.out; \
	 find $$PARENT/$(ZIP_NAME) -name "t2-*.pdf" -delete; \
	 find $$PARENT/$(ZIP_NAME) -name "*.bin" -delete; \
	 find $$PARENT/$(ZIP_NAME) -name "*.lzw" -delete; \
	 find $$PARENT/$(ZIP_NAME) -name "*.huf" -delete; \
	 find $$PARENT/$(ZIP_NAME) -name "*.zip" -delete; \
	 rm -f $$PARENT/$(ZIP_NAME).zip; \
	 cd $$PARENT && zip -r $(ZIP_NAME).zip $(ZIP_NAME); \
	 rm -rf $$PARENT/$(ZIP_NAME)
	@echo "[zip] Pronto: ../$(ZIP_NAME).zip"
