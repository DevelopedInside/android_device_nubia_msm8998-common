# Verity
PRODUCT_SUPPORTS_BOOT_SIGNER := true
PRODUCT_SUPPORTS_VERITY := true
PRODUCT_SUPPORTS_VERITY_FEC := true

PRODUCT_VERITY_SIGNING_KEY := build/target/product/security/verity

PRODUCT_PACKAGES += \
    verity_key
