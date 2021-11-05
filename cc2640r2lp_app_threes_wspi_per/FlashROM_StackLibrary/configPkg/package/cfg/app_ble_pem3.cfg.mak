# invoke SourceDir generated makefile for app_ble.pem3
app_ble.pem3: .libraries,app_ble.pem3
.libraries,app_ble.pem3: package/cfg/app_ble_pem3.xdl
	$(MAKE) -f C:\Users\felipe\workspace_cc3235andcc2640\cc2640r2lp_app_threes_wspi_per\TOOLS/src/makefile.libs

clean::
	$(MAKE) -f C:\Users\felipe\workspace_cc3235andcc2640\cc2640r2lp_app_threes_wspi_per\TOOLS/src/makefile.libs clean

