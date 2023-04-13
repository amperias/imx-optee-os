#include <io.h>
#include <kernel/pseudo_ta.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <mm/tee_pager.h>
#include <mm/tee_mm.h>
#include <string.h>
#include <trace.h>
#include <kernel/huk_subkey.h>
#include <kernel/tee_common_otp.h>
#include <trace.h>
#include <utee_defines.h>
#include <bootdata/bootdata.h>
#include <crypto/crypto.h>
#include <string.h>
#include <tee/tee_cryp_utl.h>

#include <pta_boot_data.h>

#define DEBUG 0
#define SHA1_HASH_SIZE                  20 
#define SHA256_HASH_SIZE                32


static TEE_Result pta_get_device_id(uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	DMSG("Start pta_get_device_id in BOOT_DATA_PTA");
	struct tee_hw_unique_key huk = { };
	TEE_Result res = TEE_SUCCESS;

	res = tee_otp_get_hw_unique_key(&huk);

	if(res != TEE_SUCCESS)
	{
		EMSG("tee_otp_get_hw_unique_key returned 0x%x\n",
				(unsigned int)res);
		return res;
	}
	memcpy(&params[0].memref.buffer, huk.data, HW_UNIQUE_KEY_LENGTH);
	params[0].memref.size = HW_UNIQUE_KEY_LENGTH;

	if(DEBUG)
	{
		for (int i = 0; i < HW_UNIQUE_KEY_LENGTH; i++)
		{
		    DMSG("%02X", huk.data[i]);
		}

	}
	return res;
}

static TEE_Result pta_get_kernel_hash(uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res = TEE_SUCCESS;
	vaddr_t src_vaddr = 0;

	// Get virtual address from the physical kernel address
	src_vaddr = (vaddr_t)phys_to_virt(CFG_TEE_BOOT_DATA_START, MEM_AREA_IO_SEC, 2 + BOOT_DATA_HEADER_SIZE);
	DMSG("Physical = %lX, Virtual = %lX\n", (unsigned long)CFG_TEE_BOOT_DATA_START, (unsigned long)src_vaddr);

	if (!src_vaddr) {
		EMSG("Not enough memory mapped");
		return TEE_ERROR_OUT_OF_MEMORY;
	}

	uint8_t kernel_hash[SHA1_HASH_SIZE+ 1];
	res = tee_hash_createdigest(TEE_ALG_SHA1, (char*)src_vaddr, BOOT_DATA_HEADER_SIZE, kernel_hash, SHA1_HASH_SIZE);
	if (res != TEE_SUCCESS)
	{
		DMSG("tee_hash_createdigest(TEE_ALG_SHA1) Fail\n");
		goto EXIT;
	}
	else {
		DMSG("tee_hash_createdigest(TEE_ALG_SHA1) Works\n");
	}

	if(DEBUG)
	{
		DMSG("HASH VALUE:");
		for (size_t i = 0; i < SHA1_HASH_SIZE; i++)
		{
			DMSG("%02x", kernel_hash[i]);
		}
	}

	memcpy(params[1].memref.buffer, kernel_hash, SHA1_HASH_SIZE);
	params[1].memref.size = SHA1_HASH_SIZE;

EXIT:

	return res;
}

static TEE_Result invoke_command(void *session_context __unused,
		uint32_t cmd_id,
		uint32_t param_types,
		TEE_Param params[TEE_NUM_PARAMS])
{
	TEE_Result res = TEE_SUCCESS;

	DMSG("command entry point[%d] for \"%s\"\n\n\n", cmd_id, PTA_BOOT_DATA_NAME);

	switch (cmd_id) {
		case PTA_BOOT_DATA_DEVICE_ID:
			res = pta_get_device_id(param_types, params);
			break;
		case PTA_BOOT_DATA_KERNEL_HASH:
			res = pta_get_kernel_hash(param_types, params);
			break;
		default:
			EMSG("cmd: %d Not supported %s", cmd_id, PTA_BOOT_DATA_NAME);
			res = TEE_ERROR_NOT_SUPPORTED;
			break;
	}

	return res;
}

pseudo_ta_register(.uuid = PTA_BOOT_DATA_UUID,
		.name = PTA_BOOT_DATA_NAME,
		.flags = PTA_DEFAULT_FLAGS,
		.invoke_command_entry_point = invoke_command);