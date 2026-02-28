RM				= rm -rf
DOXYFILE_PATH	= Doxyfile
INDEX_HTML_PATH	= Documentation/html/index.html
DOC_IMAGE		= kfs-doxygen

doc:
	@ docker build --quiet -f docs.dockerfile -t $(DOC_IMAGE) .
	@ docker run --rm -v "$(shell pwd)/../":/work -w /work/Documentation $(DOC_IMAGE) doxygen $(DOXYFILE_PATH)
	@ echo "\nDocument: $(INDEX_HTML_PATH)\n"

fclean:
	@ $(RM) html/

.PHONY:	doc fclean
