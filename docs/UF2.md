# UF2 Support

Implemented to this version of the spec:
https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md

## Notes

- In a UF2 file with multiple targets, the `blockNo` and `numBlocks` block
  fields should be specific and independent for each target
- The DAPLink will programme "flash data" from the following block types:
    - Blocks without the Family ID flag
    - Blocks with the optional `target_cfg.uf2_family_id` Family ID configured
        - This struct value is optional and not all targets has it configured
    - Blocks with the DAPLink family ID, where the 2 MSBytes are `0xDA91`
      and the 2 LSBytes are the Board Id, i.e. `0xDA910000 | (uint16_t)board_id`
- In a UF2 files with more than one compatible Family ID, DAPLink will only
  flash blocks for the first match it finds.
- Data blocks without the Family ID flag will always be flashed
- The validation to detect if a UF2 block is compatible can be overwritten
  per target
- A UF2 file without blocks for this target will raise the
  `ERROR_IAP_UPDT_INCOMPLETE` error

## WIP

- A way for DAPLink to distinguish between DAPLink Interface and Target blocks
- There is currently an issue flashing UF2 files with blocks for different families
- Page erasing and programming might not work (WIP needs to be tested)

## Features not implemented

- [File containers](https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md#file-containers) 
- [UF2.TXT](https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md#files-exposed-by-bootloaders)
- [CURRENT.TXT](https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md#files-exposed-by-bootloaders)
- [INDEX.HTM](https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md#files-exposed-by-bootloaders)
- [MD5 checksum](https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md#md5-checksum)
- [Extension tags](https://github.com/microsoft/uf2/blob/d03b585ed780ed51bb0d1e6e8cf233aacb408305/README.md#extension-tags)
