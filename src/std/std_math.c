#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

static RisaValue risa_std_math_min (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_max (void*, uint8_t, RisaValue*);

void risa_std_register_math(RisaVM* vm) {
    #define STD_MATH_OBJ_ENTRY(name) , RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, risa_dense_native_value(risa_std_math_##name)

    RisaDenseObject* objMath = risa_dense_object_create_under(vm, 2
                                                              STD_MATH_OBJ_ENTRY(min)
                                                              STD_MATH_OBJ_ENTRY(max));

    risa_vm_global_set(vm, "math", sizeof("math") - 1, RISA_DENSE_VALUE((RisaDenseValue *) objMath));

    #undef STD_STRING_ENTRY
}

static RisaValue risa_std_math_min(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return RISA_NULL_VALUE;

    struct {
        RisaValueType type;

        union {
            uint8_t byte;
            int64_t integer;
            double floating;
        } as;
    } min;

    switch(args[0].type) {
        case RISA_VAL_BYTE:
            min.type = RISA_VAL_BYTE;
            min.as.byte = RISA_AS_BYTE(args[0]);
            break;
        case RISA_VAL_INT:
            min.type = RISA_VAL_INT;
            min.as.integer = RISA_AS_INT(args[0]);
            break;
        case RISA_VAL_FLOAT:
            min.type = RISA_VAL_FLOAT;
            min.as.floating = RISA_AS_FLOAT(args[0]);
            break;
        default:
            return RISA_NULL_VALUE;
    }

    for(uint8_t i = 1; i < argc; ++i) {
        switch(args[i].type) {
            case RISA_VAL_BYTE:
                switch(min.type) {
                    case RISA_VAL_BYTE:
                        if(RISA_AS_BYTE(args[i]) < min.as.byte) {
                            min.as.byte = RISA_AS_BYTE(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(RISA_AS_BYTE(args[i]) < min.as.integer) {
                            min.type = RISA_VAL_BYTE;
                            min.as.byte = RISA_AS_BYTE(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(RISA_AS_BYTE(args[i]) < min.as.floating) {
                            min.type = RISA_VAL_BYTE;
                            min.as.byte = RISA_AS_BYTE(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_INT:
                switch(min.type) {
                    case RISA_VAL_BYTE:
                        if(RISA_AS_INT(args[i]) < min.as.byte) {
                            min.type = RISA_VAL_INT;
                            min.as.integer = RISA_AS_INT(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(RISA_AS_INT(args[i]) < min.as.integer) {
                            min.as.integer = RISA_AS_INT(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(RISA_AS_INT(args[i]) < min.as.floating) {
                            min.type = RISA_VAL_INT;
                            min.as.integer = RISA_AS_INT(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_FLOAT:
                switch(min.type) {
                    case RISA_VAL_BYTE:
                        if(RISA_AS_FLOAT(args[i]) < min.as.byte) {
                            min.type = RISA_VAL_FLOAT;
                            min.as.floating = RISA_AS_FLOAT(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(RISA_AS_FLOAT(args[i]) < min.as.integer) {
                            min.type = RISA_VAL_FLOAT;
                            min.as.floating = RISA_AS_FLOAT(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(RISA_AS_FLOAT(args[i]) < min.as.floating) {
                            min.as.floating = RISA_AS_FLOAT(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                return RISA_NULL_VALUE;
        }
    }

    switch(min.type) {
        case RISA_VAL_BYTE:
            return RISA_BYTE_VALUE(min.as.byte);
        case RISA_VAL_INT:
            return RISA_INT_VALUE(min.as.integer);
        case RISA_VAL_FLOAT:
            return RISA_FLOAT_VALUE(min.as.floating);
        default:
            return RISA_NULL_VALUE;
    }
}

static RisaValue risa_std_math_max(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return RISA_NULL_VALUE;

    struct {
        RisaValueType type;

        union {
            uint8_t byte;
            int64_t integer;
            double floating;
        } as;
    } max;

    switch(args[0].type) {
        case RISA_VAL_BYTE:
            max.type = RISA_VAL_BYTE;
            max.as.byte = RISA_AS_BYTE(args[0]);
            break;
        case RISA_VAL_INT:
            max.type = RISA_VAL_INT;
            max.as.integer = RISA_AS_INT(args[0]);
            break;
        case RISA_VAL_FLOAT:
            max.type = RISA_VAL_FLOAT;
            max.as.floating = RISA_AS_FLOAT(args[0]);
            break;
        default:
            return RISA_NULL_VALUE;
    }

    for(uint8_t i = 1; i < argc; ++i) {
        switch(args[i].type) {
            case RISA_VAL_BYTE:
                switch(max.type) {
                    case RISA_VAL_BYTE:
                        if(RISA_AS_BYTE(args[i]) > max.as.byte) {
                            max.as.byte = RISA_AS_BYTE(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(RISA_AS_BYTE(args[i]) > max.as.integer) {
                            max.type = RISA_VAL_BYTE;
                            max.as.byte = RISA_AS_BYTE(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(RISA_AS_BYTE(args[i]) > max.as.floating) {
                            max.type = RISA_VAL_BYTE;
                            max.as.byte = RISA_AS_BYTE(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_INT:
                switch(max.type) {
                    case RISA_VAL_BYTE:
                        if(RISA_AS_INT(args[i]) > max.as.byte) {
                            max.type = RISA_VAL_INT;
                            max.as.integer = RISA_AS_INT(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(RISA_AS_INT(args[i]) > max.as.integer) {
                            max.as.integer = RISA_AS_INT(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(RISA_AS_INT(args[i]) > max.as.floating) {
                            max.type = RISA_VAL_INT;
                            max.as.integer = RISA_AS_INT(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_FLOAT:
                switch(max.type) {
                    case RISA_VAL_BYTE:
                        if(RISA_AS_FLOAT(args[i]) > max.as.byte) {
                            max.type = RISA_VAL_FLOAT;
                            max.as.floating = RISA_AS_FLOAT(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(RISA_AS_FLOAT(args[i]) > max.as.integer) {
                            max.type = RISA_VAL_FLOAT;
                            max.as.floating = RISA_AS_FLOAT(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(RISA_AS_FLOAT(args[i]) > max.as.floating) {
                            max.as.floating = RISA_AS_FLOAT(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                return RISA_NULL_VALUE;
        }
    }

    switch(max.type) {
        case RISA_VAL_BYTE:
            return RISA_BYTE_VALUE(max.as.byte);
        case RISA_VAL_INT:
            return RISA_INT_VALUE(max.as.integer);
        case RISA_VAL_FLOAT:
            return RISA_FLOAT_VALUE(max.as.floating);
        default:
            return RISA_NULL_VALUE;
    }
}