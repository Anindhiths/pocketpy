#include "pocketpy/interpreter/frame.h"
#include "pocketpy/objects/object.h"

void ValueStack__ctor(ValueStack* self) {
    self->sp = self->begin;
    self->end = self->begin + PK_VM_STACK_SIZE;
}

void ValueStack__clear(ValueStack* self) {
    self->sp = self->begin;
}

py_TValue* FastLocals__try_get_by_name(py_TValue* locals, const CodeObject* co, py_Name name){
    int index = c11_smallmap_n2i__get(&co->varnames_inv, name, -1);
    if(index == -1) return NULL;
    return &locals[index];
}

pk_NameDict* FastLocals__to_namedict(py_TValue* locals, const CodeObject* co) {
    pk_NameDict* dict = pk_NameDict__new();
    c11__foreach(c11_smallmap_n2i_KV, &co->varnames_inv, entry) {
        py_TValue value = locals[entry->value];
        if(!py_isnull(&value)){
            pk_NameDict__set(dict, entry->key, value);
        }
    }
    return dict;
}

UnwindTarget* UnwindTarget__new(UnwindTarget* next, int iblock, int offset){
    UnwindTarget* self = malloc(sizeof(UnwindTarget));
    self->next = next;
    self->iblock = iblock;
    self->offset = offset;
    return self;
}

void UnwindTarget__delete(UnwindTarget* self){
    free(self);
}

Frame* Frame__new(const CodeObject* co, const py_TValue* module, const py_TValue* function, py_TValue* p0, py_TValue* locals, const CodeObject* locals_co){
    static_assert(sizeof(Frame) <= kPoolFrameBlockSize, "!(sizeof(Frame) <= kPoolFrameBlockSize)");
    Frame* self = PoolFrame_alloc();
    self->f_back = NULL;
    self->ip = (Bytecode*)co->codes.data - 1;
    self->co = co;
    self->module = module->_obj;
    self->function = function ? function->_obj : NULL;
    self->p0 = p0;
    self->locals = locals;
    self->locals_co = locals_co;
    self->uw_list = NULL;
    return self;
}

void Frame__delete(Frame* self){
    while(self->uw_list) {
        UnwindTarget* p = self->uw_list;
        self->uw_list = p->next;
        UnwindTarget__delete(p);
    }
    PoolFrame_dealloc(self);
}

int Frame__prepare_jump_exception_handler(Frame* self, ValueStack* _s){
    // try to find a parent try block
    int iblock = Frame__iblock(self);
    while(iblock >= 0) {
        CodeBlock* block = c11__at(CodeBlock, &self->co->blocks, iblock);
        if(block->type == CodeBlockType_TRY_EXCEPT) break;
        iblock = block->parent;
    }
    if(iblock < 0) return -1;
    py_TValue obj = *--_s->sp;  // pop exception object
    UnwindTarget* uw = Frame__find_unwind_target(self, iblock);
    _s->sp = (self->locals + uw->offset);  // unwind the stack                          
    *(_s->sp++) = obj;      // push it back
    return c11__at(CodeBlock, &self->co->blocks, iblock)->end;
}

void Frame__prepare_jump_break(Frame* self, ValueStack* _s, int target){
    int iblock = Frame__iblock(self);
    if(target >= self->co->codes.count) {
        while(iblock >= 0) iblock = Frame__exit_block(self, _s, iblock);
    } else {
        // BUG (solved)
        // for i in range(4):
        //     _ = 0
        // # if there is no op here, the block check will fail
        // while i: --i
        int next_block = c11__at(BytecodeEx, &self->co->codes_ex, target)->iblock;
        while(iblock >= 0 && iblock != next_block)
            iblock = Frame__exit_block(self, _s, iblock);
        assert(iblock == next_block);
    }
}

int Frame__prepare_loop_break(Frame* self, ValueStack* _s){
    int iblock = Frame__iblock(self);
    int target = c11__getitem(CodeBlock, &self->co->blocks, iblock).end;
    Frame__prepare_jump_break(self, _s, target);
    return target;
}

int Frame__exit_block(Frame* self, ValueStack* _s, int iblock){
    CodeBlock* block = c11__at(CodeBlock, &self->co->blocks, iblock);
    if(block->type == CodeBlockType_FOR_LOOP) {
        _s->sp--;  // pop iterator
    } else if(block->type == CodeBlockType_CONTEXT_MANAGER) {
        _s->sp--;  // pop context variable
    }
    return block->parent;
}

UnwindTarget* Frame__find_unwind_target(Frame* self, int iblock){
    UnwindTarget* uw;
    for(uw = self->uw_list; uw; uw = uw->next) {
        if(uw->iblock == iblock) return uw;
    }
    return NULL;
}

void Frame__set_unwind_target(Frame* self, py_TValue* sp) {
    int iblock = Frame__iblock(self);
    UnwindTarget* existing = Frame__find_unwind_target(self, iblock);
    if(existing) {
        existing->offset = sp - self->locals;
    } else {
        UnwindTarget* prev = self->uw_list;
        self->uw_list = UnwindTarget__new(prev, iblock, sp - self->locals);
    }
}

py_TValue* Frame__f_closure_try_get(Frame* self, py_Name name){
    // if(self->function == NULL) return NULL;
    // pkpy::Function* fn = PyObject__as(pkpy::Function, self->function);
    // if(fn->_closure == nullptr) return nullptr;
    // return pk_NameDict__try_get(fn->_closure, name);
    return NULL;
}