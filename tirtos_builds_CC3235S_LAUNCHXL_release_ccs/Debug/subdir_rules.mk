################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
build-2131322433:
	@$(MAKE) --no-print-directory -Onone -f subdir_rules.mk build-2131322433-inproc

build-2131322433-inproc: ../release.cfg
	@echo 'Building file: "$<"'
	@echo 'Invoking: XDCtools'
	"C:/ti/xdctools_3_51_03_28_core/xs" --xdcpath="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source;C:/ti/simplelink_cc32xx_sdk_3_20_00_06/kernel/tirtos/packages;" xdc.tools.configuro -o configPkg -t ti.targets.arm.elf.M4 -p ti.platforms.simplelink:CC3220S -r release -c "C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS" "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

configPkg/linker.cmd: build-2131322433 ../release.cfg
configPkg/compiler.opt: build-2131322433
configPkg/: build-2131322433


