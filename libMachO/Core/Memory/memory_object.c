//----------------------------------------------------------------------------//
//|
//|             MachOKit - A Lightweight Mach-O Parsing Library
//|             memory_object.c
//|
//|             D.V.
//|             Copyright (c) 2014-2015 D.V. All rights reserved.
//|
//| Permission is hereby granted, free of charge, to any person obtaining a
//| copy of this software and associated documentation files (the "Software"),
//| to deal in the Software without restriction, including without limitation
//| the rights to use, copy, modify, merge, publish, distribute, sublicense,
//| and/or sell copies of the Software, and to permit persons to whom the
//| Software is furnished to do so, subject to the following conditions:
//|
//| The above copyright notice and this permission notice shall be included
//| in all copies or substantial portions of the Software.
//|
//| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
//| OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
//| MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
//| IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
//| CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
//| TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//| SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//----------------------------------------------------------------------------//

#include "core_internal.h"

//----------------------------------------------------------------------------//
#pragma mark -  Classes
//----------------------------------------------------------------------------//

//|++++++++++++++++++++++++++++++++++++|//
static mk_context_t*
__mk_memory_object_get_context(mk_type_ref self)
{ return mk_type_get_context( mk_memory_map_for_object(self).type ); }

const struct mk_memory_object_vtable _mk_memory_object_class = {
    .base.super                 = &_mk_type_class,
    .base.name                  = "memory_object",
    .base.get_context           = &__mk_memory_object_get_context,
};

//---------------------------------------------------------------------------//
#pragma mark -  Instance Methods
//---------------------------------------------------------------------------//

//|++++++++++++++++++++++++++++++++++++|//
vm_address_t mk_memory_object_address(mk_memory_object_ref mobj)
{ return mobj.memory_object->address; }

//|++++++++++++++++++++++++++++++++++++|//
vm_size_t mk_memory_object_length(mk_memory_object_ref mobj)
{ return mobj.memory_object->length; }

//|++++++++++++++++++++++++++++++++++++|//
mk_vm_address_t mk_memory_object_base_address(mk_memory_object_ref mobj)
{ return mobj.memory_object->context_address; }

//|++++++++++++++++++++++++++++++++++++|//
mk_vm_range_t mk_memory_object_range(mk_memory_object_ref mobj)
{ return mk_vm_range_make(mk_memory_object_address(mobj), mk_memory_object_length(mobj)); }

//|++++++++++++++++++++++++++++++++++++|//
mk_vm_range_t mk_memory_object_context_range(mk_memory_object_ref mobj)
{ return mk_vm_range_make(mk_memory_object_base_address(mobj), mk_memory_object_length(mobj)); }

//|++++++++++++++++++++++++++++++++++++|//
bool
mk_memory_object_verify_local_pointer(mk_memory_object_ref mobj, vm_offset_t offset, vm_address_t address, vm_size_t length, mk_error_t* error)
{
    mk_context_t *ctx = mk_type_get_context(mobj.type);
    
    // Verify that the offset value won't overrun a native pointer
    if (UINTPTR_MAX - offset < address) {
        _mkl_error(ctx, "Adding input offset %" PRIuPTR " to input address 0x%" PRIxPTR " would overflow.", (uintptr_t)offset, (uintptr_t)address);
        MK_ERROR_OUT = MK_EOVERFLOW;
        return false;
    }
    
    // Adjust the address using the verified offset
    address += offset;
    
    // Verify that the address value won't overflow
    if (UINTPTR_MAX - length < address) {
        _mkl_error(ctx, "Adding input length %" PRIuPTR " to offset input address 0x%" PRIxPTR " would overflow.", (uintptr_t)length, (uintptr_t)address);
        MK_ERROR_OUT = MK_EOVERFLOW;
        return false;
    }
    
    vm_address_t mobj_address = mk_memory_object_address(mobj);
    vm_size_t mobj_length = mk_memory_object_length(mobj);
    
    // mobj_address + mobj_length overflow check should have been performed
    // at creation time.
    
    // Verify that the address starts within range
    if (address < mobj_address) {
        _mkl_error(ctx, "Input range (offset address = 0x%" PRIxPTR ", length = %" PRIuPTR ") is not within <mk_memory_object_t %p (base = 0x%" PRIxPTR ", length = %" PRIuPTR ")>", (uintptr_t)address, (uintptr_t)length, mobj.type, (uintptr_t)mobj_address, (uintptr_t)mobj_length);
        MK_ERROR_OUT = MK_EOUT_OF_RANGE;
        return false;
    }
    
    // Check that the block ends within range
    if (mobj_address + mobj_length < address + length) {
        _mkl_error(ctx, "Input range (offset address = 0x%" PRIxPTR ", length = %" PRIuPTR ") is not within <mk_memory_object_t %p (base = 0x%" PRIxPTR ", length = %" PRIuPTR ")>", (uintptr_t)address, (uintptr_t)length, mobj.type, (uintptr_t)mobj_address, (uintptr_t)mobj_length);
        MK_ERROR_OUT = MK_EOUT_OF_RANGE;
        return false;
    }
    
    MK_ERROR_OUT = MK_ESUCCESS;
    return true;
}

//|++++++++++++++++++++++++++++++++++++|//
vm_address_t
mk_memory_object_remap_address(mk_memory_object_ref mobj, mk_vm_offset_t offset, mk_vm_address_t address, mk_vm_size_t length, mk_error_t* error)
{
    mk_error_t err;
    mk_context_t *ctx = mk_type_get_context(mobj.type);
    
    // Adjust the address using the verified offset
    if ((err = mk_vm_address_apply_offset(address, offset, &address))) {
        _mkl_error(ctx, "[mk_memory_object] Adding input offset %" MK_VM_PRIiOFFSET " to input address 0x%" MK_VM_PRIxADDR " would overflow.", offset, address);
        MK_ERROR_OUT = err;
        return UINTPTR_MAX;
    }
    
    // Verify that the address value won't overflow
    if (UINTPTR_MAX - length < address) {
        _mkl_error(ctx, "[mk_memory_object] Adding input length %" MK_VM_PRIiSIZE " to offset input address 0x%" MK_VM_PRIxADDR " would overflow.", length, address);
        MK_ERROR_OUT = MK_EOVERFLOW;
        return UINTPTR_MAX;
    }
    
    mk_vm_address_t mobj_context_address = mk_memory_object_base_address(mobj);
    vm_address_t mobj_address = mk_memory_object_address(mobj);
    vm_size_t mobj_length = mk_memory_object_length(mobj);
    
    // mobj_context_address + mobj_length overflow check should have been
    // performed at creation time.
    
    // Verify that the address starts within range
    if (address < mobj_context_address) {
        _mkl_error(ctx, "Input range (offset address = 0x%" MK_VM_PRIxADDR ", length = %" MK_VM_PRIuSIZE ") is not within <mk_memory_object_t %p (base = 0x%" MK_VM_PRIxADDR ", length = %" PRIuPTR ")>", address, length, mobj.type, mobj_context_address, (uintptr_t)mobj_length);
        MK_ERROR_OUT = MK_EOUT_OF_RANGE;
        return UINTPTR_MAX;
    }
    
    // Verify that the block ends within range
    if (mobj_context_address + mobj_length < address + length) {
        _mkl_error(ctx, "Input range (offset address = 0x%" MK_VM_PRIxADDR ", length = %" MK_VM_PRIuSIZE ") is not within <mk_memory_object_t %p (base = 0x%" MK_VM_PRIxADDR ", length = %" PRIuPTR ")>", address, length, mobj.type, mobj_context_address, (uintptr_t)mobj_length);
        MK_ERROR_OUT = MK_EOUT_OF_RANGE;
        return UINTPTR_MAX;
    }
    
    // Adding slide to mobj_address is safe since slide can not be greater
    // than the length of mobj.
    mk_vm_offset_t slide = address - mobj_context_address;
    
    MK_ERROR_OUT = MK_ESUCCESS;
    return (vm_address_t)(mobj_address + slide);
}

//|++++++++++++++++++++++++++++++++++++|//
mk_vm_address_t
mk_memory_object_unmap_address(mk_memory_object_ref mobj, vm_offset_t offset, vm_address_t address, vm_size_t length, mk_error_t* error)
{
    if (!mk_memory_object_verify_local_pointer(mobj, address, offset, length, error))
        return MK_VM_ADDRESS_INVALID;
    
    mk_vm_address_t mobj_context_address = mk_memory_object_base_address(mobj);
    vm_address_t mobj_process_address = mk_memory_object_address(mobj);
    
    // _mk_memory_object_verify_local_pointer already verified
    // (address + offset) will not overflow.
    
    // _mk_memory_object_verify_local_pointer already verified
    // (address + offset) is > mobj_process_address.  Underflow can not occur.
    
    vm_offset_t slide = (address + offset) - mobj_process_address;
    
    // (mobj_context_address + slide) can not overflow.
    
    MK_ERROR_OUT = MK_ESUCCESS;
    return mobj_context_address + slide;
}

//|++++++++++++++++++++++++++++++++++++|//
uint8_t
mk_memory_object_read_byte(mk_memory_object_ref mobj, mk_vm_offset_t offset, mk_vm_address_t address, mk_data_model_ref data_model, mk_error_t *error)
{
#pragma unused (data_model)
    vm_address_t remapped_address = mk_memory_object_remap_address(mobj, offset, address, sizeof(uint8_t), error);
    if (remapped_address == UINTPTR_MAX)
        return 0;

    MK_ERROR_OUT = MK_ESUCCESS;
    return *(uint8_t*)remapped_address;
}

//|++++++++++++++++++++++++++++++++++++|//
uint16_t
mk_memory_object_read_word(mk_memory_object_ref mobj, mk_vm_offset_t offset, mk_vm_address_t address, mk_data_model_ref data_model, mk_error_t *error)
{
    vm_address_t remapped_address = mk_memory_object_remap_address(mobj, offset, address, sizeof(uint16_t), error);
    if (remapped_address == UINTPTR_MAX)
        return 0;
    
    MK_ERROR_OUT = MK_ESUCCESS;
    if (data_model.data_model)
        return mk_data_model_get_byte_order(data_model)->swap16( *(uint16_t*)remapped_address );
    else
        return *(uint16_t*)remapped_address;
}

//|++++++++++++++++++++++++++++++++++++|//
uint32_t
mk_memory_object_read_dword(mk_memory_object_ref mobj, mk_vm_offset_t offset, mk_vm_address_t address, mk_data_model_ref data_model, mk_error_t *error)
{
    vm_address_t remapped_address = mk_memory_object_remap_address(mobj, offset, address, sizeof(uint32_t), error);
    if (remapped_address == UINTPTR_MAX)
        return 0;
    
    MK_ERROR_OUT = MK_ESUCCESS;
    if (data_model.data_model)
        return mk_data_model_get_byte_order(data_model)->swap32( *(uint32_t*)remapped_address );
    else
        return *(uint32_t*)remapped_address;
}

//|++++++++++++++++++++++++++++++++++++|//
uint64_t
mk_memory_object_read_qword(mk_memory_object_ref mobj, mk_vm_offset_t offset, mk_vm_address_t address, mk_data_model_ref data_model, mk_error_t *error)
{
    vm_address_t remapped_address = mk_memory_object_remap_address(mobj, offset, address, sizeof(uint64_t), error);
    if (remapped_address == UINTPTR_MAX)
        return 0;
    
    MK_ERROR_OUT = MK_ESUCCESS;
    if (data_model.data_model)
        return mk_data_model_get_byte_order(data_model)->swap64( *(uint64_t*)remapped_address );
    else
        return *(uint64_t*)remapped_address;
}
