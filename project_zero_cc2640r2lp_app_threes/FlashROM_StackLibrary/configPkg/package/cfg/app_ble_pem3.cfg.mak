# invoke SourceDir generated makefile for app_ble.pem3
app_ble.pem3: .libraries,app_ble.pem3
.libraries,app_ble.pem3: package/cfg/app_ble_pem3.xdl
	$(MAKE) -f C:\Users\felipe\workspace_cc3235andcc2640\project_zero_cc2640r2lp_app_threes\TOOLS/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\felipe\workspace_cc3235andcc2640\project_zero_cc2640r2lp_app_threes\TOOLS/src/makefile.libs clean

