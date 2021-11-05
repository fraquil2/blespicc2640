# invoke SourceDir generated makefile for release.pem4
release.pem4: .libraries,release.pem4
.libraries,release.pem4: package/cfg/release_pem4.xdl
	$(MAKE) -f C:\Users\felipe\workspace_cc3235andcc2640\tirtos_builds_CC3235S_LAUNCHXL_release_ccs/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\felipe\workspace_cc3235andcc2640\tirtos_builds_CC3235S_LAUNCHXL_release_ccs/src/makefile.libs clean

