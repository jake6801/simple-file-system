all: diskinfo disklist diskput

diskinfo: diskinfo.c
	gcc diskinfo.c -o diskinfo

disklist: disklist.c
	gcc disklist.c -o disklist

diskput: diskput.c 
	gcc diskput.c -o diskput

clean: 
	rm -f diskinfo disklist diskput