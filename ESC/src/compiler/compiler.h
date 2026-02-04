#ifndef ES_COMPILER_H
#define ES_COMPILER_H

#include "frontend/lexer/tokenizer.h"
#include "frontend/parser/parser.h"
#include "frontend/parser/ast.h"
#include "frontend/semantic/semantic_analyzer.h"
#include "frontend/semantic/symbol_table.h"
#include "frontend/semantic/generics.h"
#include "middle/ir/ir.h"
#include "middle/codegen/type_checker.h"
#include "backend/x86/x86_codegen.h"
#include "driver/compiler.h"
#include "driver/parallel_compiler.h"
#include "driver/preprocessor.h"
#include "driver/project.h"

#endif 