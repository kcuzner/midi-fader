/* Storage located at the end of flash */
.storage :
{
    . = ALIGN(4);
    _storagea_magic = .;
    KEEP(*(.storage.magic))
    . = _storagea_magic + 4;
    _sstoragea = .;
    KEEP(*(.storage))
    FILL(0xFF)
    . = _storagea_magic + _storage_size / 2;
    _storageb_magic = .;
    FILL(0xFF)
    . = _storageb_magic + 4;
    _sstorageb = .;
    . = _storagea_magic + _storage_size;
    _estorage = .;
} > FLASH_STORAGE

