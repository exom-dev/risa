#include "std.h"
#include "../value/value.h"
#include "../def/macro.h"

#include <math.h>
#include <errno.h>

static RisaValue risa_std_math_min   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_max   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_floor (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_ceil  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_abs   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_map   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_sin   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_cos   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_tan   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_asin  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_acos  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_atan  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_atan2 (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_log   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_pow   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_exp   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_sqrt  (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_deg   (void*, uint8_t, RisaValue*);
static RisaValue risa_std_math_rad   (void*, uint8_t, RisaValue*);

static double risa_std_math_internal_adjust_result (double);

void risa_std_register_math(RisaVM* vm) {
    #define STD_MATH_OBJ_FN_ENTRY(name) , RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, risa_dense_native_value(risa_std_math_##name)
    #define STD_MATH_OBJ_FLOAT_ENTRY(name, val) , RISA_STRINGIFY_DIRECTLY(name), sizeof(RISA_STRINGIFY_DIRECTLY(name)) - 1, val

    RisaDenseObject* objMath = risa_dense_object_create_under(vm, 21
                                                              STD_MATH_OBJ_FN_ENTRY(min)
                                                              STD_MATH_OBJ_FN_ENTRY(max)
                                                              STD_MATH_OBJ_FN_ENTRY(floor)
                                                              STD_MATH_OBJ_FN_ENTRY(ceil)
                                                              STD_MATH_OBJ_FN_ENTRY(abs)
                                                              STD_MATH_OBJ_FN_ENTRY(map)
                                                              STD_MATH_OBJ_FN_ENTRY(sin)
                                                              STD_MATH_OBJ_FN_ENTRY(cos)
                                                              STD_MATH_OBJ_FN_ENTRY(tan)
                                                              STD_MATH_OBJ_FN_ENTRY(asin)
                                                              STD_MATH_OBJ_FN_ENTRY(acos)
                                                              STD_MATH_OBJ_FN_ENTRY(atan)
                                                              STD_MATH_OBJ_FN_ENTRY(atan2)
                                                              STD_MATH_OBJ_FN_ENTRY(log)
                                                              STD_MATH_OBJ_FN_ENTRY(pow)
                                                              STD_MATH_OBJ_FN_ENTRY(exp)
                                                              STD_MATH_OBJ_FN_ENTRY(sqrt)
                                                              STD_MATH_OBJ_FN_ENTRY(deg)
                                                              STD_MATH_OBJ_FN_ENTRY(rad)
                                                              STD_MATH_OBJ_FLOAT_ENTRY(pi, risa_value_from_float(RISA_MATH_PI))
                                                              STD_MATH_OBJ_FLOAT_ENTRY(e, risa_value_from_float(RISA_MATH_E)));

    risa_vm_global_set(vm, "math", sizeof("math") - 1, risa_value_from_dense((RisaDenseValue *) objMath));

    #undef STD_STRING_FLOATENTRY
}



static RisaValue risa_std_math_min(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

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
            min.as.byte = risa_value_as_byte(args[0]);
            break;
        case RISA_VAL_INT:
            min.type = RISA_VAL_INT;
            min.as.integer = risa_value_as_int(args[0]);
            break;
        case RISA_VAL_FLOAT:
            min.type = RISA_VAL_FLOAT;
            min.as.floating = risa_value_as_float(args[0]);
            break;
        default:
            return risa_value_from_null();
    }

    for(uint8_t i = 1; i < argc; ++i) {
        switch(args[i].type) {
            case RISA_VAL_BYTE:
                switch(min.type) {
                    case RISA_VAL_BYTE:
                        if(risa_value_as_byte(args[i]) < min.as.byte) {
                            min.as.byte = risa_value_as_byte(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(risa_value_as_byte(args[i]) < min.as.integer) {
                            min.type = RISA_VAL_BYTE;
                            min.as.byte = risa_value_as_byte(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(risa_value_as_byte(args[i]) < min.as.floating) {
                            min.type = RISA_VAL_BYTE;
                            min.as.byte = risa_value_as_byte(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_INT:
                switch(min.type) {
                    case RISA_VAL_BYTE:
                        if(risa_value_as_int(args[i]) < min.as.byte) {
                            min.type = RISA_VAL_INT;
                            min.as.integer = risa_value_as_int(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(risa_value_as_int(args[i]) < min.as.integer) {
                            min.as.integer = risa_value_as_int(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(risa_value_as_int(args[i]) < min.as.floating) {
                            min.type = RISA_VAL_INT;
                            min.as.integer = risa_value_as_int(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_FLOAT:
                switch(min.type) {
                    case RISA_VAL_BYTE:
                        if(risa_value_as_float(args[i]) < min.as.byte) {
                            min.type = RISA_VAL_FLOAT;
                            min.as.floating = risa_value_as_float(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(risa_value_as_float(args[i]) < min.as.integer) {
                            min.type = RISA_VAL_FLOAT;
                            min.as.floating = risa_value_as_float(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(risa_value_as_float(args[i]) < min.as.floating) {
                            min.as.floating = risa_value_as_float(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                return risa_value_from_null();
        }
    }

    switch(min.type) {
        case RISA_VAL_BYTE:
            return risa_value_from_byte(min.as.byte);
        case RISA_VAL_INT:
            return risa_value_from_int(min.as.integer);
        case RISA_VAL_FLOAT:
            return risa_value_from_float(min.as.floating);
        default:
            return risa_value_from_null();
    }
}

static RisaValue risa_std_math_max(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

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
            max.as.byte = risa_value_as_byte(args[0]);
            break;
        case RISA_VAL_INT:
            max.type = RISA_VAL_INT;
            max.as.integer = risa_value_as_int(args[0]);
            break;
        case RISA_VAL_FLOAT:
            max.type = RISA_VAL_FLOAT;
            max.as.floating = risa_value_as_float(args[0]);
            break;
        default:
            return risa_value_from_null();
    }

    for(uint8_t i = 1; i < argc; ++i) {
        switch(args[i].type) {
            case RISA_VAL_BYTE:
                switch(max.type) {
                    case RISA_VAL_BYTE:
                        if(risa_value_as_byte(args[i]) > max.as.byte) {
                            max.as.byte = risa_value_as_byte(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(risa_value_as_byte(args[i]) > max.as.integer) {
                            max.type = RISA_VAL_BYTE;
                            max.as.byte = risa_value_as_byte(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(risa_value_as_byte(args[i]) > max.as.floating) {
                            max.type = RISA_VAL_BYTE;
                            max.as.byte = risa_value_as_byte(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_INT:
                switch(max.type) {
                    case RISA_VAL_BYTE:
                        if(risa_value_as_int(args[i]) > max.as.byte) {
                            max.type = RISA_VAL_INT;
                            max.as.integer = risa_value_as_int(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(risa_value_as_int(args[i]) > max.as.integer) {
                            max.as.integer = risa_value_as_int(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(risa_value_as_int(args[i]) > max.as.floating) {
                            max.type = RISA_VAL_INT;
                            max.as.integer = risa_value_as_int(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            case RISA_VAL_FLOAT:
                switch(max.type) {
                    case RISA_VAL_BYTE:
                        if(risa_value_as_float(args[i]) > max.as.byte) {
                            max.type = RISA_VAL_FLOAT;
                            max.as.floating = risa_value_as_float(args[i]);
                        }
                        break;
                    case RISA_VAL_INT:
                        if(risa_value_as_float(args[i]) > max.as.integer) {
                            max.type = RISA_VAL_FLOAT;
                            max.as.floating = risa_value_as_float(args[i]);
                        }
                        break;
                    case RISA_VAL_FLOAT:
                        if(risa_value_as_float(args[i]) > max.as.floating) {
                            max.as.floating = risa_value_as_float(args[i]);
                        }
                        break;
                    default:
                        break;
                }
                break;
            default:
                return risa_value_from_null();
        }
    }

    switch(max.type) {
        case RISA_VAL_BYTE:
            return risa_value_from_byte(max.as.byte);
        case RISA_VAL_INT:
            return risa_value_from_int(max.as.integer);
        case RISA_VAL_FLOAT:
            return risa_value_from_float(max.as.floating);
        default:
            return risa_value_from_null();
    }
}

static RisaValue risa_std_math_floor(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = floor(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();
    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_ceil(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = ceil(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();
    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_abs(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = fabs(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();
    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_map(void* vm, uint8_t argc, RisaValue* args) {
    if(argc < 5)
        return risa_value_from_null();

    for(uint8_t i = 0; i < 5; ++i)
        if(!risa_value_is_num(args[i]))
            return risa_value_from_null();

    double x = risa_value_as_float(args[0]);
    double start = risa_value_as_float(args[1]);
    double end = risa_value_as_float(args[2]);
    double newStart = risa_value_as_float(args[3]);
    double newEnd = risa_value_as_float(args[4]);

    if(end == start)
        return risa_value_from_null();

    double result = ((x - start) / (end - start)) * (newEnd - newStart) + newStart;

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_sin(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = sin(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_cos(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = cos(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_tan(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = tan(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_asin(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    double x = risa_value_as_float(args[0]);

    if(x < -1 || x > 1)
        return risa_value_from_null();

    errno = 0;

    double result = asin(x);

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_acos(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    double x = risa_value_as_float(args[0]);

    if(x < -1 || x > 1)
        return risa_value_from_null();

    errno = 0;

    double result = acos(x);

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_atan(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = atan(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_atan2(void* vm, uint8_t argc, RisaValue* args) {
    if(argc < 2)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]) || !risa_value_is_num(args[1]))
        return risa_value_from_null();

    double y = risa_value_as_float(args[0]);
    double x = risa_value_as_float(args[1]);

    if(y == 0 && x == 0) {
        return risa_value_from_float(0);
    }

    errno = 0;

    double result = atan2(y, x);

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_log(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    if(argc == 1) {
        double x = risa_value_as_float(args[0]);

        if(x <= 0)
            return risa_value_from_null();

        return risa_value_from_float(log(x));
    }

    if(!risa_value_is_num(args[1]))
        return risa_value_from_null();

    double base = risa_value_as_float(args[0]);
    double x = risa_value_as_float(args[1]);

    if(base <= 0 || base == 1 || x <= 0)
        return risa_value_from_null();

    errno = 0;

    double result = log(x) / log(base);

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_pow(void* vm, uint8_t argc, RisaValue* args) {
    if(argc < 2)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]) || !risa_value_is_num(args[1]))
        return risa_value_from_null();

    double base = risa_value_as_float(args[0]);
    double exponent = risa_value_as_float(args[1]);

    if(base < 0 && (exponent > 0 && exponent < 1))
        return risa_value_from_null();

    errno = 0;

    double result = pow(base, exponent);

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_exp(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    errno = 0;

    double result = exp(risa_value_as_float(args[0]));

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_sqrt(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    double arg = risa_value_as_float(args[0]);

    if(arg < 0)
        return risa_value_from_null();

    errno = 0;

    double result = sqrt(arg);

    if(errno != 0)
        return risa_value_from_null();

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_deg(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    double result = risa_value_as_float(args[0]) * RISA_MATH_RAD2DEG;

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static RisaValue risa_std_math_rad(void* vm, uint8_t argc, RisaValue* args) {
    if(argc == 0)
        return risa_value_from_null();

    if(!risa_value_is_num(args[0]))
        return risa_value_from_null();

    double result = risa_value_as_float(args[0]) * RISA_MATH_DEG2RAD;

    return risa_value_from_float(risa_std_math_internal_adjust_result(result));
}

static double risa_std_math_internal_adjust_result(double result) {
    if(result > 0 && result < RISA_VALUE_FLOAT_ZERO_THRESHOLD)
        return 0;
    return result;
}