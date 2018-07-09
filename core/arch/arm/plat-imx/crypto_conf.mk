#
# Define the cryptographic algorithm to be built
#

ifeq ($(CFG_IMX_CAAM), y)
$(call force, CFG_CRYPTO_WITH_HW_ACC,y)
$(call force, CFG_IMXCRYPT,y)

#
# Define the TomCrypt as the Software library used to do
# algorithm not done by the HW
#
$(call force, CFG_IMXCRYPT_TOMCRYPT,y)
endif

ifeq ($(CFG_IMXCRYPT), y)
$(call force, CFG_CRYPTO_RNG_HW,y)
$(call force, CFG_CRYPTO_HASH_HW,y)
$(call force, CFG_CRYPTO_CIPHER_HW,y)

$(call force, CFG_CRYPTO_CCM_HW,n)
$(call force, CFG_CRYPTO_GCM_HW,n)

$(call force, CFG_CRYPTO_PKCS_HW,n)
$(call force, CFG_CRYPTO_PK_HW,n)
$(call force, CFG_CRYPTO_CMAC_HW,y)

# Asymmetric ciphers
$(call force, CFG_CRYPTO_DSA,n)
$(call force, CFG_CRYPTO_DH,n)
$(call force, CFG_CRYPTO_ECC,n)

# Authenticated encryption
$(call force, CFG_CRYPTO_CCM,n)

endif

#
# Definition of the HASH Algorithm supported by HW
#
ifeq ($(CFG_CRYPTO_HASH_HW), y)
CFG_CRYPTO_HASH_HW_MD5    ?= y
CFG_CRYPTO_HASH_HW_SHA1   ?= y
CFG_CRYPTO_HASH_HW_SHA224 ?= y
CFG_CRYPTO_HASH_HW_SHA256 ?= y
CFG_CRYPTO_HASH_HW_SHA384 ?= n
CFG_CRYPTO_HASH_HW_SHA512 ?= n
endif

cryp-full-hw-enabled =												\
	$(call cfg-all-enabled, 										\
		$(patsubst %, CFG_CRYPTO_$(strip $(1))_HW_%, $(strip $(2))))

$(call force, CFG_CRYPTO_HMAC_FULL_HW, $(call cryp-full-hw-enabled, HASH, \
	MD5 SHA1 SHA224 SHA256 SHA384 SHA512))

