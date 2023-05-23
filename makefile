########## Path for uCOS-II core source files #################################
UCOS_SRC=c:\software\ucos-II\source

########## Path for uCOS-II WIN32 port source files ###########################
UCOS_PORT_SRC=c:\software\ucos-II\ports\80x86\win32\vc\src

########## Path for uCOS-II WIN32 example source files ########################
UCOS_PORT_EX=.\

########## Name of Example source file ########################################
EXAMPLE=test.c

all:	
	@cl -nologo /MD /I$(UCOS_SRC) /I$(UCOS_PORT_SRC) /I$(UCOS_PORT_EX) $(EXAMPLE) $(UCOS_SRC)\ucos_ii.c $(UCOS_PORT_SRC)\pc.c $(UCOS_PORT_SRC)\os_cpu_c.c  winmm.lib user32.lib

clean:
	@if exist *.obj del *.obj
	@if exist *.bak del *.bak
	@if exist *.pdb del *.pdb
	@if exist *.ilk del *.ilk
	@if exist *.log del *.log	
	