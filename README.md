This LLVM pass goes over all the loops in a file and prints out the following:

1.  The number of loops with a formulated condition variable. This is based on the ScalarEvolutionWrapperPass analysis and whether the back edge taken count is invariant
2.  The number of loops that have bounded iterations. This is basically a subset of the above and applies to the loops that the upper bound can easily be computed e.g. for loops with a fixed upper bound
3.  The number of loops that have an InductionVariable within implying that probably there is a counter in the loop (could be a subset of 1)
4.  The number of possibly infinite loops that is basically all the loops that do not meet the above conditions
5.  Along with the above, the line numbers, the function name and the file names would be printed

Note that the above outputs are based on the cfg analysis (not interprocedural). To run the analysis on a large project with a large number of code files, simply change the Makefile to use clang compiler and produce LLVM IR.

To change the compiler, you need to change the CC environment variable, the linker environment variable and do a few other things to produce ll files instead of.o. Below is an example for “vsftpd” Makefile project:

        # Makefile for systems with GNU tools
        CC 	=	clang
        LD      =       llvm-ld
        INSTALL	=	install
        IFLAGS  = -idirafter dummyinc
        #CFLAGS = -g
                CFLAGS	=	-g -S -emit-llvm -Wno-enum-conversion -O2 -fPIE -fstack-protector --param=ssp-buffer-size=4 \
        	-Wall -W -Wshadow -Werror -Wformat-security \
        	-D_FORTIFY_SOURCE=2 \
        	#-pedantic -Wconversion
        
                LIBS	=	`./vsf_findlibs.sh`
        LINK	=	-Wl,-s
                LDFLAGS	=	-fPIE -pie -Wl,-z,relro -Wl,-z,now
        
                OBJS	=	main.ll utility.ll prelogin.ll ftpcmdio.ll postlogin.ll privsock.ll \
        		tunables.ll ftpdataio.ll secbuf.ll ls.ll \
        		postprivparent.ll logging.ll str.ll netstr.ll sysstr.ll strlist.ll \
            banner.ll filestr.ll parseconf.ll secutil.ll \
            ascii.ll oneprocess.ll twoprocess.ll privops.ll standalone.ll hash.ll \
            tcpwrap.ll ipaddrparse.ll access.ll features.ll readwrite.ll opts.ll \
            ssl.ll sslslave.ll ptracesandbox.ll ftppolicy.ll sysutil.ll sysdeputil.ll \
            seccompsandbox.ll
        
        
        %.ll:  %.c
                $(CC) -c $*.c $(CFLAGS) $(IFLAGS)
        
        vsftpd: $(OBJS)
        $(CC) $(OBJS) $(LINK) $(LDFLAGS) $(LIBS)
        
        install:
        if [ -x /usr/local/sbin ]; then \
        		$(INSTALL) -m 755 vsftpd /usr/local/sbin/vsftpd; \
        	else \
        		$(INSTALL) -m 755 vsftpd /usr/sbin/vsftpd; fi
        if [ -x /usr/local/man ]; then \
        		$(INSTALL) -m 644 vsftpd.8 /usr/local/man/man8/vsftpd.8; \
        		$(INSTALL) -m 644 vsftpd.conf.5 /usr/local/man/man5/vsftpd.conf.5; \
        	elif [ -x /usr/share/man ]; then \
        		$(INSTALL) -m 644 vsftpd.8 /usr/share/man/man8/vsftpd.8; \
        		$(INSTALL) -m 644 vsftpd.conf.5 /usr/share/man/man5/vsftpd.conf.5; \
        	else \
        		$(INSTALL) -m 644 vsftpd.8 /usr/man/man8/vsftpd.8; \
        		$(INSTALL) -m 644 vsftpd.conf.5 /usr/man/man5/vsftpd.conf.5; fi
        if [ -x /etc/xinetd.d ]; then \
        		$(INSTALL) -m 644 xinetd.d/vsftpd /etc/xinetd.d/vsftpd; fi
        
                clean:
        rm -f *.o *.ll *.swp vsftpd
        
Note that I added “-g -S -emit-llvm -Wno-enum-conversion” to the CFLAGS and changed to target %.o to %.ll:  %.c to produce ll files.
After running the above, you will get ll files that contain the llvm IR. Then you can use the shell script (run chmod +x to give permission to the .sh file)  to produce a .csv file containing the result of analysis.
The .so library is produced for llvm (clang) 9. But you should be able to compile it for other versions as well. Just comy the pass source codes to the llvm project folder and run the following in the build folder:

        cd [project]/program
        cmake clean -DCMAKE_BUILD_TYPE=Release ../
        cd [project]/build
        Make -j4
        
 After preparing the llvm optimization pass library, and the .ll files, cd to the directory containing .ll files and run the loopProfiler sript:
 
        ./loopProfiler.sh &> vsftpd_loops_2.csv
 
 Make sure loopProfiler script points to the correct path to the .so file.
 The output looks like this:

        function name,Infinite loops,Bounded loops,Formulated loop condition,	Nested loops,Inductive loops LoC,loops in total,file name

        vsf_access_check_file, 0, 0, 0, 0, 0, 0, NA, access.ll
        vsf_access_check_file_visible, 0, 0, 0, 0, 0, 0, NA, access.ll
        vsf_ascii_ascii_to_bin, 0, 0, 1, 0, 1, 1, 33, ascii.ll
