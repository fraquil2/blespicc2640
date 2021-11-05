################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

# Each subdirectory must supply rules for building sources it contributes
sbl/sbl.obj: C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source/ti/sbl/sbl.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="C:/Users/felipe/workspace_cc3235andcc2640/ble_wifi_provisioning_CC3235S_LAUNCHXL_tirtos_ccs" --include_path="C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source/ti/posix/ccs" --include_path="C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include" -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="sbl/$(basename $(<F)).d_raw" --obj_directory="sbl" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

sbl/sbl_cmd.obj: C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source/ti/sbl/sbl_cmd.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="C:/Users/felipe/workspace_cc3235andcc2640/ble_wifi_provisioning_CC3235S_LAUNCHXL_tirtos_ccs" --include_path="C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source/ti/posix/ccs" --include_path="C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include" -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="sbl/$(basename $(<F)).d_raw" --obj_directory="sbl" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

sbl/sbl_image_int.obj: C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source/ti/sbl/sbl_image_int.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="C:/Users/felipe/workspace_cc3235andcc2640/ble_wifi_provisioning_CC3235S_LAUNCHXL_tirtos_ccs" --include_path="C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source/ti/posix/ccs" --include_path="C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include" -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="sbl/$(basename $(<F)).d_raw" --obj_directory="sbl" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

sbl/sbl_simplelink.obj: C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source/ti/sbl/sbl_simplelink.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="C:/Users/felipe/workspace_cc3235andcc2640/ble_wifi_provisioning_CC3235S_LAUNCHXL_tirtos_ccs" --include_path="C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source/ti/posix/ccs" --include_path="C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include" -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="sbl/$(basename $(<F)).d_raw" --obj_directory="sbl" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '

sbl/sbl_tl.obj: C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source/ti/sbl/sbl_tl.c $(GEN_OPTS) | $(GEN_FILES) $(GEN_MISC_FILES)
	@echo 'Building file: "$<"'
	@echo 'Invoking: Arm Compiler'
	"C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib -me --include_path="C:/Users/felipe/workspace_cc3235andcc2640/ble_wifi_provisioning_CC3235S_LAUNCHXL_tirtos_ccs" --include_path="C:/ti/simplelink_sdk_ble_plugin_3_20_00_24/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source" --include_path="C:/ti/simplelink_cc32xx_sdk_3_20_00_06/source/ti/posix/ccs" --include_path="C:/ti/ccs1040/ccs/tools/compiler/ti-cgt-arm_20.2.5.LTS/include" -g --diag_warning=225 --diag_warning=255 --diag_wrap=off --display_error_number --gen_func_subsections=on --preproc_with_compile --preproc_dependency="sbl/$(basename $(<F)).d_raw" --obj_directory="sbl" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: "$<"'
	@echo ' '


