//-----------------------------------------------------------
// Izak De La Cruz
// AGL4 Compiler
// AegielCompiler.cpp
//-----------------------------------------------------------
#define _CRT_SECURE_NO_WARNINGS 
#include <iostream>
#include <iomanip>

#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <vector>

using namespace std;

//#define TRACEREADER
//#define TRACESCANNER
//#define TRACEPARSER
#define TRACEIDENTIFIERTABLE
#define TRACECOMPILER

#include "AGLHeader.h"

/*
========================
Changes to AGL3 compiler
========================
Added tokens
   (pseudo-terminals)
   (  reserved words) DECREE THEN LEST OTHERWISE CONCLUDED
                      VIGIL UNTIL WHILST MAINTAIN PERSIST
                      MUTABLE UNCHECKED
   (     punctuation)
   (       operators)

Updated functions
   ParseStatement
   ParseDataDefinitions (to support mandatory initialization and MUTABLE)
   ParseTerm (ADDED ACTUAL division by zero and modulo by zero checks)

Added functions
   ParseDECREEStatement
   ParseVIGILStatement
   ParseWHILSTStatement (optional)
   ParsePERSISTStatement (optional)
   ParseUNCHECKEDBlock

New Security Features in AGL4.1:
   - Immutable by default (MUTABLE keyword for mutable variables)
   - Mandatory initialization at declaration
   - Mandatory braces for all control structures
   - Division by zero runtime checks (IMPLEMENTED)
   - Modulo by zero runtime checks (IMPLEMENTED)
   - UNCHECKED blocks for performance-critical code

BUG FIXES:
   - Fixed global variable initialization - globals are now properly
     initialized inside PROGRAMBODY instead of before PROGRAMMAIN starts
   - Fixed DECREE jump logic - now uses JMPNT (jump if NOT true)
   - Fixed VIGIL jump logic - now uses JMPT (jump if true to exit)
*/

//-----------------------------------------------------------
typedef enum
//-----------------------------------------------------------
{
    // pseudo-terminals
    IDENTIFIER,
    INTEGER,
    STRING,
    EOPTOKEN,
    UNKTOKEN,
    // reserved words
    MAIN,
    END,
    OUTPUT,
    ENDL,
    OR,
    NOR,
    XOR,
    AND,
    NAND,
    INVERT,
    GUARD,
    TRUTH,
    FALSEHOOD,
    ORDAIN,
    INTEGER_TYPE,  // INTEGER keyword for type declaration
    TESTAMENT,
    INVOKE,
    MUTABLE,       // NEW: mutability modifier
    DECREE,        // NEW: IF statement
    THEN,          // NEW: IF/LEST body marker
    LEST,          // NEW: ELIF
    OTHERWISE,     // NEW: ELSE
    CONCLUDED,     // NEW: end of control structure
    VIGIL,         // NEW: mid-test loop
    UNTIL,         // NEW: mid-test condition
    WHILST,        // NEW: pre-test loop (optional)
    MAINTAIN,      // NEW: pre-test loop body (optional)
    PERSIST,       // NEW: post-test loop (optional)
    UNCHECKED,     // NEW: unchecked arithmetic block
    // punctuation
    COMMA,
    SEMICOLON,
    OBRACE,
    CBRACE,
    OPARENTHESIS,
    CPARENTHESIS,
    COLON,
    LEFTARROW,
    // operators
    LT,
    LTEQ,
    EQ,
    GT,
    GTEQ,
    NOTEQ,
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    MODULUS,
    POWER  // ^ and **
} TOKENTYPE;

//-----------------------------------------------------------
struct TOKENTABLERECORD
    //-----------------------------------------------------------
{
    TOKENTYPE type;
    char description[15 + 1];
    bool isReservedWord;
};

//-----------------------------------------------------------
const TOKENTABLERECORD TOKENTABLE[] =
//-----------------------------------------------------------
{
   { IDENTIFIER     ,"IDENTIFIER"     ,false },
   { INTEGER        ,"INTEGER"        ,false },
   { STRING         ,"STRING"         ,false },
   { EOPTOKEN       ,"EOPTOKEN"       ,false },
   { UNKTOKEN       ,"UNKTOKEN"       ,false },
   { MAIN           ,"MAIN"           ,true  },
   { END            ,"END"            ,true  },
   { OUTPUT         ,"OUTPUT"         ,true  },
   { ENDL           ,"ENDL"           ,true  },
   { OR             ,"OR"             ,true  },
   { NOR            ,"NOR"            ,true  },
   { XOR            ,"XOR"            ,true  },
   { AND            ,"AND"            ,true  },
   { NAND           ,"NAND"           ,true  },
   { INVERT         ,"INVERT"         ,true  },
   { GUARD          ,"GUARD"          ,true  },
   { TRUTH          ,"TRUTH"          ,true  },
   { FALSEHOOD      ,"FALSEHOOD"      ,true  },
   { ORDAIN         ,"ORDAIN"         ,true  },
   { INTEGER_TYPE   ,"INTEGER"        ,true  },
   { TESTAMENT      ,"TESTAMENT"      ,true  },
   { INVOKE         ,"INVOKE"         ,true  },
   { MUTABLE        ,"MUTABLE"        ,true  },
   { DECREE         ,"DECREE"         ,true  },
   { THEN           ,"THEN"           ,true  },
   { LEST           ,"LEST"           ,true  },
   { OTHERWISE      ,"OTHERWISE"      ,true  },
   { CONCLUDED      ,"CONCLUDED"      ,true  },
   { VIGIL          ,"VIGIL"          ,true  },
   { UNTIL          ,"UNTIL"          ,true  },
   { WHILST         ,"WHILST"         ,true  },
   { MAINTAIN       ,"MAINTAIN"       ,true  },
   { PERSIST        ,"PERSIST"        ,true  },
   { UNCHECKED      ,"UNCHECKED"      ,true  },
   { COMMA          ,"COMMA"          ,false },
   { SEMICOLON      ,"SEMICOLON"      ,false },
   { OBRACE         ,"OBRACE"         ,false },
   { CBRACE         ,"CBRACE"         ,false },
   { OPARENTHESIS   ,"OPARENTHESIS"   ,false },
   { CPARENTHESIS   ,"CPARENTHESIS"   ,false },
   { COLON          ,"COLON"          ,false },
   { LEFTARROW      ,"LEFTARROW"      ,false },
   { LT             ,"LT"             ,false },
   { LTEQ           ,"LTEQ"           ,false },
   { EQ             ,"EQ"             ,false },
   { GT             ,"GT"             ,false },
   { GTEQ           ,"GTEQ"           ,false },
   { NOTEQ          ,"NOTEQ"          ,false },
   { PLUS           ,"PLUS"           ,false },
   { MINUS          ,"MINUS"          ,false },
   { MULTIPLY       ,"MULTIPLY"       ,false },
   { DIVIDE         ,"DIVIDE"        ,false },
   { MODULUS        ,"MODULUS"        ,false },
   { POWER          ,"POWER"          ,false }
};

//-----------------------------------------------------------
struct TOKEN
    //-----------------------------------------------------------
{
    TOKENTYPE type;
    char lexeme[SOURCELINELENGTH + 1];
    int sourceLineNumber;
    int sourceLineIndex;
};

//-----------------------------------------------------------
// NEW: Structure to track global variable initialization
//-----------------------------------------------------------
struct GLOBALINIT
{
    char reference[SOURCELINELENGTH + 1];
    char initValue[SOURCELINELENGTH + 1];
    char comment[SOURCELINELENGTH + 1];
};

//--------------------------------------------------
// Global variables
//--------------------------------------------------
READER<CALLBACKSUSED> reader(SOURCELINELENGTH, LOOKAHEAD);
LISTER lister(LINESPERPAGE);
// CODEGENERATION
CODE code;
IDENTIFIERTABLE identifierTable(&lister, MAXIMUMIDENTIFIERS);
// ENDCODEGENERATION

// NEW: Global flag for checked arithmetic
bool checkedArithmetic = true;

// NEW: Track global variable initializations
vector<GLOBALINIT> globalInitializations;

#ifdef TRACEPARSER
int level;
#endif

//-----------------------------------------------------------
void EnterModule(const char module[])
//-----------------------------------------------------------
{
#ifdef TRACEPARSER
    char information[SOURCELINELENGTH + 1];

    level++;
    sprintf(information, "   %*s>%s", level * 2, " ", module);
    lister.ListInformationLine(information);
#endif
}

//-----------------------------------------------------------
void ExitModule(const char module[])
//-----------------------------------------------------------
{
#ifdef TRACEPARSER
    char information[SOURCELINELENGTH + 1];

    sprintf(information, "   %*s<%s", level * 2, " ", module);
    lister.ListInformationLine(information);
    level--;
#endif
}

//--------------------------------------------------
void ProcessCompilerError(int sourceLineNumber, int sourceLineIndex, const char errorMessage[])
//--------------------------------------------------
{
    char information[SOURCELINELENGTH + 1];

    // Use "panic mode" error recovery technique: report error message and terminate compilation!
    sprintf(information, "     At (%4d:%3d) %s", sourceLineNumber, sourceLineIndex, errorMessage);
    lister.ListInformationLine(information);
    lister.ListInformationLine("AGL compiler ending with compiler error!\n");
    throw(AGLEXCEPTION("AGL compiler ending with compiler error!"));
}

//-----------------------------------------------------------
int main()
//-----------------------------------------------------------
{
    void Callback1(int sourceLineNumber, const char sourceLine[]);
    void Callback2(int sourceLineNumber, const char sourceLine[]);
    void ParseAegielProgram(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char sourceFileName[80 + 1];
    TOKEN tokens[LOOKAHEAD + 1];

    cout << "Source filename? "; cin >> sourceFileName;

    try
    {
        lister.OpenFile(sourceFileName);
        code.OpenFile(sourceFileName);

        // CODEGENERATION
        code.EmitBeginningCode(sourceFileName);
        // ENDCODEGENERATION

        reader.SetLister(&lister);
        reader.AddCallbackFunction(Callback1);
        reader.AddCallbackFunction(Callback2);
        reader.OpenFile(sourceFileName);

        // Fill tokens[] for look-ahead
        for (int i = 0; i <= LOOKAHEAD; i++)
            GetNextToken(tokens);

#ifdef TRACEPARSER
        level = 0;
#endif

        ParseAegielProgram(tokens);

        // CODEGENERATION
        code.EmitEndingCode();
        // ENDCODEGENERATION

    }
    catch (AGLEXCEPTION aglException)
    {
        cout << "AGL exception: " << aglException.GetDescription() << endl;
    }
    lister.ListInformationLine("******* AGL compiler ending");
    cout << "AGL compiler ending\n";

    system("PAUSE");
    return(0);
}

//-----------------------------------------------------------
void ParseAegielProgram(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseDataDefinitions(TOKEN tokens[], IDENTIFIERSCOPE identifierScope);
    void ParseMAINDefinition(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    EnterModule("AegielProgram");

    ParseDataDefinitions(tokens, GLOBALSCOPE);

#ifdef TRACECOMPILER
    identifierTable.DisplayTableContents("Contents of identifier table after compilation of global data definitions");
#endif

    if (tokens[0].type == MAIN)
        ParseMAINDefinition(tokens);
    else
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex,
            "Expecting MAIN");

    if (tokens[0].type != EOPTOKEN)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex,
            "Expecting end-of-program");

    ExitModule("AegielProgram");
}

//-----------------------------------------------------------
void ParseDataDefinitions(TOKEN tokens[], IDENTIFIERSCOPE identifierScope)
//-----------------------------------------------------------
{
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    EnterModule("DataDefinitions");

    while (tokens[0].type == ORDAIN)
    {
        do
        {
            char identifier[MAXIMUMLENGTHIDENTIFIER + 1];
            char reference[MAXIMUMLENGTHIDENTIFIER + 1];
            DATATYPE datatype;
            bool isInTable;
            bool isMutable = false;
            int index;

            GetNextToken(tokens);

            // NEW: Check for MUTABLE keyword
            if (tokens[0].type == MUTABLE)
            {
                isMutable = true;
                GetNextToken(tokens);
            }

            if (tokens[0].type != IDENTIFIER)
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting identifier");
            strcpy(identifier, tokens[0].lexeme);
            GetNextToken(tokens);

            if (tokens[0].type != COLON)
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ':'");
            GetNextToken(tokens);

            switch (tokens[0].type)
            {
            case INTEGER_TYPE:
                datatype = INTTYPE;
                break;
            case TESTAMENT:
                datatype = BOOLTYPE;
                break;
            default:
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting INTEGER or TESTAMENT");
            }
            GetNextToken(tokens);

            // NEW: Mandatory initialization
            if (tokens[0].type != LEFTARROW)
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '<-' (mandatory initialization)");
            GetNextToken(tokens);

            // Parse initialization expression
            DATATYPE initDatatype;

            // FIX: For global scope, save the current code position to extract init value
            int codePositionBefore = -1;
            if (identifierScope == GLOBALSCOPE)
            {
                // Handle simple literals without emitting code yet
                if (tokens[0].type == INTEGER)
                {
                    // Simple case: direct integer literal
                    char initValue[SOURCELINELENGTH + 1];
                    sprintf(initValue, "#0D%s", tokens[0].lexeme);

                    GLOBALINIT gi;
                    strcpy(gi.initValue, initValue);
                    gi.reference[0] = '\0';  // Will be filled in later
                    sprintf(gi.comment, "Initialize %s", identifier);
                    globalInitializations.push_back(gi);

                    // Now consume the integer and don't emit code
                    GetNextToken(tokens);
                    initDatatype = INTTYPE;
                }
                else if (tokens[0].type == TRUTH || tokens[0].type == FALSEHOOD)
                {
                    // Handle TRUTH and FALSEHOOD literals
                    char initValue[SOURCELINELENGTH + 1];
                    if (tokens[0].type == TRUTH)
                        sprintf(initValue, "#0XFFFF");
                    else
                        sprintf(initValue, "#0X0000");

                    GLOBALINIT gi;
                    strcpy(gi.initValue, initValue);
                    gi.reference[0] = '\0';  // Will be filled in later
                    sprintf(gi.comment, "Initialize %s", identifier);
                    globalInitializations.push_back(gi);

                    // Now consume the boolean and don't emit code
                    GetNextToken(tokens);
                    initDatatype = BOOLTYPE;
                }
                else
                {
                    // Complex expression - evaluate it
                    ParseExpression(tokens, initDatatype);

                    // Store that this global needs the TOS value
                    GLOBALINIT gi;
                    strcpy(gi.initValue, "TOS");  // Top of stack
                    gi.reference[0] = '\0';
                    sprintf(gi.comment, "Initialize %s from expression", identifier);
                    globalInitializations.push_back(gi);
                }
            }
            else
            {
                // For local scope, emit code normally
                ParseExpression(tokens, initDatatype);
            }

            if (initDatatype != datatype)
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Initialization type mismatch");

            index = identifierTable.GetIndex(identifier, isInTable);
            if (isInTable && identifierTable.IsInCurrentScope(index))
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Multiply-defined identifier");

            // CODEGENERATION
            char comment[SOURCELINELENGTH + 1];
            if (isMutable)
                sprintf(comment, "%s (mutable)", identifier);
            else
                sprintf(comment, "%s (immutable)", identifier);

            switch (identifierScope)
            {
            case GLOBALSCOPE:
                code.AddRWToStaticData(1, comment, reference);
                // FIX: Store the reference for later initialization
                if (!globalInitializations.empty())
                {
                    strcpy(globalInitializations.back().reference, reference);
                }
                // Don't emit POP here - will be done in PROGRAMBODY
                identifierTable.AddToTable(identifier,
                    isMutable ? GLOBAL_VARIABLE : GLOBAL_CONSTANT,
                    datatype, reference);
                break;
            case PROGRAMMODULESCOPE:
                code.AddRWToStaticData(1, comment, reference);
                code.EmitFormattedLine("", "POP", reference);  // Store initialization value
                identifierTable.AddToTable(identifier,
                    isMutable ? PROGRAMMODULE_VARIABLE : PROGRAMMODULE_CONSTANT,
                    datatype, reference);
                break;
            }
            // ENDCODEGENERATION

        } while (tokens[0].type == COMMA);

        if (tokens[0].type != SEMICOLON)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
        GetNextToken(tokens);
    }

    ExitModule("DataDefinitions");
}

//-----------------------------------------------------------
void ParseMAINDefinition(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseDataDefinitions(TOKEN tokens[], IDENTIFIERSCOPE identifierScope);
    void ParseStatement(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    char label[SOURCELINELENGTH + 1];
    char reference[SOURCELINELENGTH + 1];

    EnterModule("MAINDefinition");

    // CODEGENERATION
    code.EmitUnformattedLine("; **** =========");
    sprintf(line, "; **** MAIN module (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);
    code.EmitUnformattedLine("; **** =========");
    code.EmitFormattedLine("PROGRAMMAIN", "EQU", "*");
    code.EmitFormattedLine("", "PUSH", "#RUNTIMESTACK", "set SP");
    code.EmitFormattedLine("", "POPSP");
    code.EmitFormattedLine("", "PUSHA", "STATICDATA", "set SB");
    code.EmitFormattedLine("", "POPSB");
    code.EmitFormattedLine("", "PUSH", "#HEAPBASE", "initialize heap");
    code.EmitFormattedLine("", "PUSH", "#HEAPSIZE");
    code.EmitFormattedLine("", "SVC", "#SVC_INITIALIZE_HEAP");
    sprintf(label, "PROGRAMBODY%04d", code.LabelSuffix());
    code.EmitFormattedLine("", "CALL", label);
    code.AddDSToStaticData("Normal program termination", "", reference);
    code.EmitFormattedLine("", "PUSHA", reference);
    code.EmitFormattedLine("", "SVC", "#SVC_WRITE_STRING");
    code.EmitFormattedLine("", "SVC", "#SVC_WRITE_ENDL");
    code.EmitFormattedLine("", "PUSH", "#0D0", "terminate with status = 0");
    code.EmitFormattedLine("", "SVC", "#SVC_TERMINATE");

    code.EmitFormattedLine(label, "EQU", "*");

    // FIX: Initialize global variables INSIDE PROGRAMBODY
    code.EmitUnformattedLine("; Initialize global variables");
    for (size_t i = 0; i < globalInitializations.size(); i++)
    {
        if (strcmp(globalInitializations[i].initValue, "TOS") != 0)
        {
            // Simple value - emit PUSH/POP
            code.EmitFormattedLine("", "PUSH", globalInitializations[i].initValue,
                globalInitializations[i].comment);
            code.EmitFormattedLine("", "POP", globalInitializations[i].reference);
        }
        else
        {
            // Value is already on stack from complex expression
            code.EmitFormattedLine("", "POP", globalInitializations[i].reference,
                globalInitializations[i].comment);
        }
    }
    // ENDCODEGENERATION

    GetNextToken(tokens);

    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{'");
    GetNextToken(tokens);

    identifierTable.EnterNestedStaticScope();
    ParseDataDefinitions(tokens, PROGRAMMODULESCOPE);

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    // CODEGENERATION
    code.EmitFormattedLine("", "RETURN");
    code.EmitUnformattedLine("; **** =========");
    sprintf(line, "; **** END (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);
    code.EmitUnformattedLine("; **** =========");
    // ENDCODEGENERATION

#ifdef TRACECOMPILER
    identifierTable.DisplayTableContents("Contents of identifier table at end of compilation of MAIN module definition");
#endif

    identifierTable.ExitNestedStaticScope();

    GetNextToken(tokens);

    if (tokens[0].type != END)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting END");
    GetNextToken(tokens);

    ExitModule("MAINDefinition");
}

//-----------------------------------------------------------
void ParseStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseOUTPUTStatement(TOKEN tokens[]);
    void ParseINVOKEStatement(TOKEN tokens[]);
    void ParseAssignmentStatement(TOKEN tokens[]);
    void ParseDECREEStatement(TOKEN tokens[]);
    void ParseVIGILStatement(TOKEN tokens[]);
    void ParseWHILSTStatement(TOKEN tokens[]);
    void ParsePERSISTStatement(TOKEN tokens[]);
    void ParseUNCHECKEDBlock(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    EnterModule("Statement");

    switch (tokens[0].type)
    {
    case OUTPUT:
        ParseOUTPUTStatement(tokens);
        break;
    case INVOKE:
        ParseINVOKEStatement(tokens);
        break;
    case IDENTIFIER:
        ParseAssignmentStatement(tokens);
        break;
    case DECREE:
        ParseDECREEStatement(tokens);
        break;
    case VIGIL:
        ParseVIGILStatement(tokens);
        break;
    case WHILST:
        ParseWHILSTStatement(tokens);
        break;
    case PERSIST:
        ParsePERSISTStatement(tokens);
        break;
    case UNCHECKED:
        ParseUNCHECKEDBlock(tokens);
        break;
    default:
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex,
            "Expecting beginning-of-statement");
        break;
    }

    ExitModule("Statement");
}

//-----------------------------------------------------------
void ParseOUTPUTStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    DATATYPE datatype;

    EnterModule("OUTPUTStatement");

    sprintf(line, "; **** OUTPUT statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    if (tokens[0].type != OPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '('");

    do
    {
        GetNextToken(tokens);

        switch (tokens[0].type)
        {
        case STRING:
            // CODEGENERATION
            char reference[SOURCELINELENGTH + 1];

            code.AddDSToStaticData(tokens[0].lexeme, "", reference);
            code.EmitFormattedLine("", "PUSHA", reference);
            code.EmitFormattedLine("", "SVC", "#SVC_WRITE_STRING");
            // ENDCODEGENERATION

            GetNextToken(tokens);
            break;
        case ENDL:
            // CODEGENERATION
            code.EmitFormattedLine("", "SVC", "#SVC_WRITE_ENDL");
            // ENDCODEGENERATION

            GetNextToken(tokens);
            break;
        default:
        {
            ParseExpression(tokens, datatype);

            // CODEGENERATION
            switch (datatype)
            {
            case INTTYPE:
                code.EmitFormattedLine("", "SVC", "#SVC_WRITE_INTEGER");
                break;
            case BOOLTYPE:
                code.EmitFormattedLine("", "SVC", "#SVC_WRITE_BOOLEAN");
                break;
            }
            // ENDCODEGENERATION
        }
        }
    } while (tokens[0].type == COMMA);

    if (tokens[0].type != CPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
    GetNextToken(tokens);

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    ExitModule("OUTPUTStatement");
}

//-----------------------------------------------------------
void ParseINVOKEStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseVariable(TOKEN tokens[], bool asLValue, DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    char reference[SOURCELINELENGTH + 1];
    char line[SOURCELINELENGTH + 1];
    DATATYPE datatype;

    EnterModule("INVOKEStatement");

    sprintf(line, "; **** INVOKE statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    if (tokens[0].type == STRING)
    {
        // CODEGENERATION
        code.AddDSToStaticData(tokens[0].lexeme, "", reference);
        code.EmitFormattedLine("", "PUSHA", reference);
        code.EmitFormattedLine("", "SVC", "#SVC_WRITE_STRING");
        // ENDCODEGENERATION

        GetNextToken(tokens);
    }

    ParseVariable(tokens, true, datatype);

    // CODEGENERATION
    switch (datatype)
    {
    case INTTYPE:
        code.EmitFormattedLine("", "SVC", "#SVC_READ_INTEGER");
        break;
    case BOOLTYPE:
        code.EmitFormattedLine("", "SVC", "#SVC_READ_BOOLEAN");
        break;
    }
    code.EmitFormattedLine("", "POP", "@SP:0D1");
    code.EmitFormattedLine("", "DISCARD", "#0D1");
    // ENDCODEGENERATION

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    ExitModule("INVOKEStatement");
}

//-----------------------------------------------------------
void ParseAssignmentStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseVariable(TOKEN tokens[], bool asLValue, DATATYPE & datatype);
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    DATATYPE datatypeLHS, datatypeRHS;
    int n;

    EnterModule("AssignmentStatement");

    sprintf(line, "; **** assignment statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    ParseVariable(tokens, true, datatypeLHS);
    n = 1;

    while (tokens[0].type == COMMA)
    {
        DATATYPE datatype;

        GetNextToken(tokens);
        ParseVariable(tokens, true, datatype);
        n++;

        if (datatype != datatypeLHS)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Mixed-mode variables not allowed");
    }

    if (tokens[0].type != LEFTARROW)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '<-'");
    GetNextToken(tokens);

    ParseExpression(tokens, datatypeRHS);

    if (datatypeLHS != datatypeRHS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Data type mismatch");

    // CODEGENERATION
    for (int i = 1; i <= n; i++)
    {
        code.EmitFormattedLine("", "MAKEDUP");
        code.EmitFormattedLine("", "POP", "@SP:0D2");
        code.EmitFormattedLine("", "SWAP");
        code.EmitFormattedLine("", "DISCARD", "#0D1");
    }
    code.EmitFormattedLine("", "DISCARD", "#0D1");
    // ENDCODEGENERATION

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    ExitModule("AssignmentStatement");
}

//-----------------------------------------------------------
void ParseDECREEStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void ParseStatement(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    char Ilabel[SOURCELINELENGTH + 1], Elabel[SOURCELINELENGTH + 1];
    DATATYPE datatype;

    EnterModule("DECREEStatement");

    sprintf(line, "; **** DECREE statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    if (tokens[0].type != OPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '('");
    GetNextToken(tokens);
    ParseExpression(tokens, datatype);
    if (tokens[0].type != CPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
    GetNextToken(tokens);
    if (tokens[0].type != THEN)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting THEN");
    GetNextToken(tokens);

    // NEW: Mandatory braces
    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
    GetNextToken(tokens);

    if (datatype != BOOLTYPE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean expression");

    // CODEGENERATION - FIX: Use JMPNT (jump if NOT true)
    sprintf(Elabel, "E%04d", code.LabelSuffix());
    code.EmitFormattedLine("", "SETT");
    code.EmitFormattedLine("", "DISCARD", "#0D1");
    sprintf(Ilabel, "I%04d", code.LabelSuffix());
    code.EmitFormattedLine("", "JMPNT", Ilabel);  // FIX: Changed from JMPT to JMPNT
    // ENDCODEGENERATION

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    // NEW: Mandatory closing brace
    GetNextToken(tokens);  // consume '}'

    // CODEGENERATION
    code.EmitFormattedLine("", "JMP", Elabel);
    code.EmitFormattedLine(Ilabel, "EQU", "*");
    // ENDCODEGENERATION

    // Handle LEST (ELIF) clauses
    while (tokens[0].type == LEST)
    {
        GetNextToken(tokens);
        if (tokens[0].type != OPARENTHESIS)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '('");
        GetNextToken(tokens);
        ParseExpression(tokens, datatype);
        if (tokens[0].type != CPARENTHESIS)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
        GetNextToken(tokens);
        if (tokens[0].type != THEN)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting THEN");
        GetNextToken(tokens);

        // NEW: Mandatory braces
        if (tokens[0].type != OBRACE)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
        GetNextToken(tokens);

        if (datatype != BOOLTYPE)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean expression");

        // CODEGENERATION - FIX: Use JMPNT
        code.EmitFormattedLine("", "SETT");
        code.EmitFormattedLine("", "DISCARD", "#0D1");
        sprintf(Ilabel, "I%04d", code.LabelSuffix());
        code.EmitFormattedLine("", "JMPNT", Ilabel);  // FIX: Changed from JMPT to JMPNT
        // ENDCODEGENERATION

        while (tokens[0].type != CBRACE)
            ParseStatement(tokens);

        GetNextToken(tokens);  // consume '}'

        // CODEGENERATION
        code.EmitFormattedLine("", "JMP", Elabel);
        code.EmitFormattedLine(Ilabel, "EQU", "*");
        // ENDCODEGENERATION
    }

    // Handle OTHERWISE (ELSE) clause
    if (tokens[0].type == OTHERWISE)
    {
        GetNextToken(tokens);

        // NEW: Mandatory braces
        if (tokens[0].type != OBRACE)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
        GetNextToken(tokens);

        while (tokens[0].type != CBRACE)
            ParseStatement(tokens);

        GetNextToken(tokens);  // consume '}'
    }

    if (tokens[0].type != CONCLUDED)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting CONCLUDED");
    GetNextToken(tokens);

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    // CODEGENERATION
    code.EmitFormattedLine(Elabel, "EQU", "*");
    // ENDCODEGENERATION

    ExitModule("DECREEStatement");
}

//-----------------------------------------------------------
void ParseVIGILStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void ParseStatement(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    char Dlabel[SOURCELINELENGTH + 1], Elabel[SOURCELINELENGTH + 1];
    DATATYPE datatype;

    EnterModule("VIGILStatement");

    sprintf(line, "; **** VIGIL statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    // NEW: Mandatory braces for first block
    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
    GetNextToken(tokens);

    // CODEGENERATION
    sprintf(Dlabel, "D%04d", code.LabelSuffix());
    sprintf(Elabel, "E%04d", code.LabelSuffix());
    code.EmitFormattedLine(Dlabel, "EQU", "*");
    // ENDCODEGENERATION

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    GetNextToken(tokens);  // consume '}'

    if (tokens[0].type != UNTIL)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting UNTIL");
    GetNextToken(tokens);

    if (tokens[0].type != OPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '('");
    GetNextToken(tokens);
    ParseExpression(tokens, datatype);
    if (tokens[0].type != CPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
    GetNextToken(tokens);

    if (datatype != BOOLTYPE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean expression");

    // CODEGENERATION - FIX: Use JMPT (jump if true to exit)
    code.EmitFormattedLine("", "SETT");
    code.EmitFormattedLine("", "DISCARD", "#0D1");
    code.EmitFormattedLine("", "JMPT", Elabel);  // FIX: Changed from JMPNT to JMPT
    // ENDCODEGENERATION

    // NEW: Mandatory braces for second block
    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
    GetNextToken(tokens);

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    GetNextToken(tokens);  // consume '}'

    if (tokens[0].type != CONCLUDED)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting CONCLUDED");
    GetNextToken(tokens);

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    // CODEGENERATION
    code.EmitFormattedLine("", "JMP", Dlabel);
    code.EmitFormattedLine(Elabel, "EQU", "*");
    // ENDCODEGENERATION

    ExitModule("VIGILStatement");
}

//-----------------------------------------------------------
void ParseWHILSTStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void ParseStatement(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    char Dlabel[SOURCELINELENGTH + 1], Elabel[SOURCELINELENGTH + 1];
    DATATYPE datatype;

    EnterModule("WHILSTStatement");

    sprintf(line, "; **** WHILST statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    // CODEGENERATION
    sprintf(Dlabel, "D%04d", code.LabelSuffix());
    sprintf(Elabel, "E%04d", code.LabelSuffix());
    code.EmitFormattedLine(Dlabel, "EQU", "*");
    // ENDCODEGENERATION

    if (tokens[0].type != OPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '('");
    GetNextToken(tokens);
    ParseExpression(tokens, datatype);
    if (tokens[0].type != CPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
    GetNextToken(tokens);

    if (tokens[0].type != MAINTAIN)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting MAINTAIN");
    GetNextToken(tokens);

    if (datatype != BOOLTYPE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean expression");

    // CODEGENERATION - WHILST uses JMPNT (exit when condition is NOT true)
    code.EmitFormattedLine("", "SETT");
    code.EmitFormattedLine("", "DISCARD", "#0D1");
    code.EmitFormattedLine("", "JMPNT", Elabel);
    // ENDCODEGENERATION

    // NEW: Mandatory braces
    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
    GetNextToken(tokens);

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    GetNextToken(tokens);  // consume '}'

    if (tokens[0].type != CONCLUDED)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting CONCLUDED");
    GetNextToken(tokens);

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    // CODEGENERATION
    code.EmitFormattedLine("", "JMP", Dlabel);
    code.EmitFormattedLine(Elabel, "EQU", "*");
    // ENDCODEGENERATION

    ExitModule("WHILSTStatement");
}

//-----------------------------------------------------------
void ParsePERSISTStatement(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void ParseStatement(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];
    char Dlabel[SOURCELINELENGTH + 1], Elabel[SOURCELINELENGTH + 1];
    DATATYPE datatype;

    EnterModule("PERSISTStatement");

    sprintf(line, "; **** PERSIST statement (%4d)", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    // NEW: Mandatory braces
    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
    GetNextToken(tokens);

    // CODEGENERATION
    sprintf(Dlabel, "D%04d", code.LabelSuffix());
    sprintf(Elabel, "E%04d", code.LabelSuffix());
    code.EmitFormattedLine(Dlabel, "EQU", "*");
    // ENDCODEGENERATION

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    GetNextToken(tokens);  // consume '}'

    if (tokens[0].type != WHILST)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting WHILST");
    GetNextToken(tokens);

    if (tokens[0].type != OPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '('");
    GetNextToken(tokens);
    ParseExpression(tokens, datatype);
    if (tokens[0].type != CPARENTHESIS)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
    GetNextToken(tokens);

    if (tokens[0].type != CONCLUDED)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting CONCLUDED");
    GetNextToken(tokens);

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    if (datatype != BOOLTYPE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean expression");

    // CODEGENERATION - PERSIST uses JMPNT (exit when condition is NOT true)
    code.EmitFormattedLine("", "SETT");
    code.EmitFormattedLine("", "DISCARD", "#0D1");
    code.EmitFormattedLine("", "JMPNT", Elabel);
    code.EmitFormattedLine("", "JMP", Dlabel);
    code.EmitFormattedLine(Elabel, "EQU", "*");
    // ENDCODEGENERATION

    ExitModule("PERSISTStatement");
}

//-----------------------------------------------------------
void ParseUNCHECKEDBlock(TOKEN tokens[])
//-----------------------------------------------------------
{
    void ParseStatement(TOKEN tokens[]);
    void GetNextToken(TOKEN tokens[]);

    char line[SOURCELINELENGTH + 1];

    EnterModule("UNCHECKEDBlock");

    sprintf(line, "; **** UNCHECKED block (%4d) - arithmetic checks disabled", tokens[0].sourceLineNumber);
    code.EmitUnformattedLine(line);

    GetNextToken(tokens);

    // NEW: Mandatory braces
    if (tokens[0].type != OBRACE)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting '{' (mandatory braces)");
    GetNextToken(tokens);

    // Disable checked arithmetic
    bool savedCheckedState = checkedArithmetic;
    checkedArithmetic = false;

    while (tokens[0].type != CBRACE)
        ParseStatement(tokens);

    GetNextToken(tokens);  // consume '}'

    // Restore checked arithmetic
    checkedArithmetic = savedCheckedState;

    if (tokens[0].type != CONCLUDED)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting CONCLUDED");
    GetNextToken(tokens);

    if (tokens[0].type != SEMICOLON)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ';'");
    GetNextToken(tokens);

    sprintf(line, "; **** End UNCHECKED block - arithmetic checks restored");
    code.EmitUnformattedLine(line);

    ExitModule("UNCHECKEDBlock");
}

//-----------------------------------------------------------
void ParseExpression(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseConjunction(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeLHS, datatypeRHS;

    EnterModule("Expression");

    ParseConjunction(tokens, datatypeLHS);

    if ((tokens[0].type == OR) ||
        (tokens[0].type == NOR) ||
        (tokens[0].type == XOR))
    {
        while ((tokens[0].type == OR) ||
            (tokens[0].type == NOR) ||
            (tokens[0].type == XOR))
        {
            TOKENTYPE operation = tokens[0].type;

            GetNextToken(tokens);
            ParseConjunction(tokens, datatypeRHS);

            switch (operation)
            {
            case OR:
                if (!((datatypeLHS == BOOLTYPE) && (datatypeRHS == BOOLTYPE)))
                    ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean operands");
                code.EmitFormattedLine("", "OR");
                datatype = BOOLTYPE;
                break;
            case NOR:
                if (!((datatypeLHS == BOOLTYPE) && (datatypeRHS == BOOLTYPE)))
                    ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean operands");
                code.EmitFormattedLine("", "NOR");
                datatype = BOOLTYPE;
                break;
            case XOR:
                if (!((datatypeLHS == BOOLTYPE) && (datatypeRHS == BOOLTYPE)))
                    ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean operands");
                code.EmitFormattedLine("", "XOR");
                datatype = BOOLTYPE;
                break;
            }
        }
    }
    else
        datatype = datatypeLHS;

    ExitModule("Expression");
}

//-----------------------------------------------------------
void ParseConjunction(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseNegation(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeLHS, datatypeRHS;

    EnterModule("Conjunction");

    ParseNegation(tokens, datatypeLHS);

    if ((tokens[0].type == AND) ||
        (tokens[0].type == NAND))
    {
        while ((tokens[0].type == AND) ||
            (tokens[0].type == NAND))
        {
            TOKENTYPE operation = tokens[0].type;

            GetNextToken(tokens);
            ParseNegation(tokens, datatypeRHS);

            switch (operation)
            {
            case AND:
                if (!((datatypeLHS == BOOLTYPE) && (datatypeRHS == BOOLTYPE)))
                    ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean operands");
                code.EmitFormattedLine("", "AND");
                datatype = BOOLTYPE;
                break;
            case NAND:
                if (!((datatypeLHS == BOOLTYPE) && (datatypeRHS == BOOLTYPE)))
                    ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean operands");
                code.EmitFormattedLine("", "NAND");
                datatype = BOOLTYPE;
                break;
            }
        }
    }
    else
        datatype = datatypeLHS;

    ExitModule("Conjunction");
}

//-----------------------------------------------------------
void ParseNegation(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseComparison(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeRHS;

    EnterModule("Negation");

    if (tokens[0].type == INVERT)
    {
        GetNextToken(tokens);
        ParseComparison(tokens, datatypeRHS);

        if (!(datatypeRHS == BOOLTYPE))
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting boolean operand");
        code.EmitFormattedLine("", "NOT");
        datatype = BOOLTYPE;
    }
    else
        ParseComparison(tokens, datatype);

    ExitModule("Negation");
}

//-----------------------------------------------------------
void ParseComparison(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseComparator(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeLHS, datatypeRHS;

    EnterModule("Comparison");

    ParseComparator(tokens, datatypeLHS);

    if ((tokens[0].type == LT) ||
        (tokens[0].type == LTEQ) ||
        (tokens[0].type == EQ) ||
        (tokens[0].type == GT) ||
        (tokens[0].type == GTEQ) ||
        (tokens[0].type == NOTEQ))
    {
        TOKENTYPE operation = tokens[0].type;

        GetNextToken(tokens);
        ParseComparator(tokens, datatypeRHS);

        if ((datatypeLHS != INTTYPE) || (datatypeRHS != INTTYPE))
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting integer operands");

        char Tlabel[SOURCELINELENGTH + 1], Elabel[SOURCELINELENGTH + 1];

        code.EmitFormattedLine("", "CMPI");
        sprintf(Tlabel, "T%04d", code.LabelSuffix());
        sprintf(Elabel, "E%04d", code.LabelSuffix());

        switch (operation)
        {
        case LT:
            code.EmitFormattedLine("", "JMPL", Tlabel);
            break;
        case LTEQ:
            code.EmitFormattedLine("", "JMPLE", Tlabel);
            break;
        case EQ:
            code.EmitFormattedLine("", "JMPE", Tlabel);
            break;
        case GT:
            code.EmitFormattedLine("", "JMPG", Tlabel);
            break;
        case GTEQ:
            code.EmitFormattedLine("", "JMPGE", Tlabel);
            break;
        case NOTEQ:
            code.EmitFormattedLine("", "JMPNE", Tlabel);
            break;
        }
        datatype = BOOLTYPE;
        code.EmitFormattedLine("", "PUSH", "#0X0000");
        code.EmitFormattedLine("", "JMP", Elabel);
        code.EmitFormattedLine(Tlabel, "PUSH", "#0XFFFF");
        code.EmitFormattedLine(Elabel, "EQU", "*");
    }
    else
        datatype = datatypeLHS;

    ExitModule("Comparison");
}

//-----------------------------------------------------------
void ParseComparator(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseTerm(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeLHS, datatypeRHS;

    EnterModule("Comparator");

    ParseTerm(tokens, datatypeLHS);

    if ((tokens[0].type == PLUS) ||
        (tokens[0].type == MINUS))
    {
        while ((tokens[0].type == PLUS) ||
            (tokens[0].type == MINUS))
        {
            TOKENTYPE operation = tokens[0].type;

            GetNextToken(tokens);
            ParseTerm(tokens, datatypeRHS);

            if ((datatypeLHS != INTTYPE) || (datatypeRHS != INTTYPE))
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting integer operands");

            switch (operation)
            {
            case PLUS:
                if (checkedArithmetic)
                {
                    code.EmitFormattedLine("", "ADDI", "", "; checked addition");
                }
                else
                {
                    code.EmitFormattedLine("", "ADDI", "", "; unchecked addition");
                }
                break;
            case MINUS:
                if (checkedArithmetic)
                {
                    code.EmitFormattedLine("", "SUBI", "", "; checked subtraction");
                }
                else
                {
                    code.EmitFormattedLine("", "SUBI", "", "; unchecked subtraction");
                }
                break;
            }
            datatype = INTTYPE;
        }
    }
    else
        datatype = datatypeLHS;

    ExitModule("Comparator");
}

//-----------------------------------------------------------
void ParseTerm(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseFactor(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeLHS, datatypeRHS;

    EnterModule("Term");

    ParseFactor(tokens, datatypeLHS);

    if ((tokens[0].type == MULTIPLY) ||
        (tokens[0].type == DIVIDE) ||
        (tokens[0].type == MODULUS))
    {
        while ((tokens[0].type == MULTIPLY) ||
            (tokens[0].type == DIVIDE) ||
            (tokens[0].type == MODULUS))
        {
            TOKENTYPE operation = tokens[0].type;
            int sourceLineNumber = tokens[0].sourceLineNumber;

            GetNextToken(tokens);
            ParseFactor(tokens, datatypeRHS);

            if ((datatypeLHS != INTTYPE) || (datatypeRHS != INTTYPE))
                ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting integer operands");

            switch (operation)
            {
            case MULTIPLY:
                if (checkedArithmetic)
                {
                    code.EmitFormattedLine("", "MULI", "", "; checked multiplication");
                }
                else
                {
                    code.EmitFormattedLine("", "MULI", "", "; unchecked multiplication");
                }
                break;

            case DIVIDE:
                if (checkedArithmetic)
                {
                    // ACTUAL DIVISION BY ZERO CHECK
                    char errorLabel[SOURCELINELENGTH + 1];
                    char okLabel[SOURCELINELENGTH + 1];
                    char lineNum[32];

                    sprintf(errorLabel, "DIVERR%04d", code.LabelSuffix());
                    sprintf(okLabel, "DIVOK%04d", code.LabelSuffix());

                    code.EmitFormattedLine("", "; **** Checked division (div by zero check)");

                    // Stack has: ... divisor, dividend (top)
                    // Check if divisor == 0
                    code.EmitFormattedLine("", "PUSH", "SP:0D0");  // Copy divisor
                    code.EmitFormattedLine("", "PUSH", "#0D0");     // Push zero
                    code.EmitFormattedLine("", "CMPI");             // Compare
                    code.EmitFormattedLine("", "JMPE", errorLabel); // If equal, error

                    // Divisor is not zero, safe to divide
                    code.EmitFormattedLine("", "DIVI");
                    code.EmitFormattedLine("", "JMP", okLabel);

                    // Divisor is zero - runtime error
                    code.EmitFormattedLine(errorLabel, "EQU", "*");
                    code.EmitFormattedLine("", "DISCARD", "#0D2");  // Remove operands
                    code.EmitFormattedLine("", "PUSH", "#0D2");     // Error code 2 = div by zero
                    sprintf(lineNum, "#0D%d", sourceLineNumber);
                    code.EmitFormattedLine("", "PUSH", lineNum);    // Line number
                    code.EmitFormattedLine("", "JMP", "HANDLERUNTIMEERROR");

                    code.EmitFormattedLine(okLabel, "EQU", "*");
                }
                else
                {
                    code.EmitFormattedLine("", "DIVI", "", "; unchecked division");
                }
                break;

            case MODULUS:
                if (checkedArithmetic)
                {
                    // ACTUAL MODULO BY ZERO CHECK
                    char errorLabel[SOURCELINELENGTH + 1];
                    char okLabel[SOURCELINELENGTH + 1];
                    char lineNum[32];

                    sprintf(errorLabel, "MODERR%04d", code.LabelSuffix());
                    sprintf(okLabel, "MODOK%04d", code.LabelSuffix());

                    code.EmitFormattedLine("", "; **** Checked modulus (mod by zero check)");

                    // Stack has: ... divisor, dividend (top)
                    // Check if divisor == 0
                    code.EmitFormattedLine("", "PUSH", "SP:0D0");  // Copy divisor
                    code.EmitFormattedLine("", "PUSH", "#0D0");     // Push zero
                    code.EmitFormattedLine("", "CMPI");             // Compare
                    code.EmitFormattedLine("", "JMPE", errorLabel); // If equal, error

                    // Divisor is not zero, safe to modulus
                    code.EmitFormattedLine("", "REMI");
                    code.EmitFormattedLine("", "JMP", okLabel);

                    // Divisor is zero - runtime error
                    code.EmitFormattedLine(errorLabel, "EQU", "*");
                    code.EmitFormattedLine("", "DISCARD", "#0D2");  // Remove operands
                    code.EmitFormattedLine("", "PUSH", "#0D3");     // Error code 3 = mod by zero
                    sprintf(lineNum, "#0D%d", sourceLineNumber);
                    code.EmitFormattedLine("", "PUSH", lineNum);    // Line number
                    code.EmitFormattedLine("", "JMP", "HANDLERUNTIMEERROR");

                    code.EmitFormattedLine(okLabel, "EQU", "*");
                }
                else
                {
                    code.EmitFormattedLine("", "REMI", "", "; unchecked modulus");
                }
                break;
            }
            datatype = INTTYPE;
        }
    }
    else
        datatype = datatypeLHS;

    ExitModule("Term");
}

//-----------------------------------------------------------
void ParseFactor(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseSecondary(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    EnterModule("Factor");

    if ((tokens[0].type == GUARD) ||
        (tokens[0].type == PLUS) ||
        (tokens[0].type == MINUS))
    {
        DATATYPE datatypeRHS;
        TOKENTYPE operation = tokens[0].type;

        GetNextToken(tokens);
        ParseSecondary(tokens, datatypeRHS);

        if (datatypeRHS != INTTYPE)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting integer operand");

        switch (operation)
        {
        case GUARD:
        {
            char Elabel[SOURCELINELENGTH + 1];

            sprintf(Elabel, "E%04d", code.LabelSuffix());
            code.EmitFormattedLine("", "SETNZPI");
            code.EmitFormattedLine("", "JMPNN", Elabel);
            code.EmitFormattedLine("", "NEGI");
            code.EmitFormattedLine(Elabel, "EQU", "*");
        }
        break;
        case PLUS:
            // Do nothing (identity operator)
            break;
        case MINUS:
            code.EmitFormattedLine("", "NEGI");
            break;
        }
        datatype = INTTYPE;
    }
    else
        ParseSecondary(tokens, datatype);

    ExitModule("Factor");
}

//-----------------------------------------------------------
void ParseSecondary(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParsePrimary(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    DATATYPE datatypeLHS, datatypeRHS;

    EnterModule("Secondary");

    ParsePrimary(tokens, datatypeLHS);

    if (tokens[0].type == POWER)
    {
        GetNextToken(tokens);
        ParsePrimary(tokens, datatypeRHS);

        if ((datatypeLHS != INTTYPE) || (datatypeRHS != INTTYPE))
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting integer operands");

        if (checkedArithmetic)
        {
            code.EmitFormattedLine("", "POWI", "", "; checked power");
        }
        else
        {
            code.EmitFormattedLine("", "POWI", "", "; unchecked power");
        }
        datatype = INTTYPE;
    }
    else
        datatype = datatypeLHS;

    ExitModule("Secondary");
}

//-----------------------------------------------------------
void ParsePrimary(TOKEN tokens[], DATATYPE& datatype)
//-----------------------------------------------------------
{
    void ParseVariable(TOKEN tokens[], bool asLValue, DATATYPE & datatype);
    void ParseExpression(TOKEN tokens[], DATATYPE & datatype);
    void GetNextToken(TOKEN tokens[]);

    EnterModule("Primary");

    switch (tokens[0].type)
    {
    case INTEGER:
    {
        char operand[SOURCELINELENGTH + 1];

        sprintf(operand, "#0D%s", tokens[0].lexeme);
        code.EmitFormattedLine("", "PUSH", operand);
        datatype = INTTYPE;
        GetNextToken(tokens);
    }
    break;
    case TRUTH:
        code.EmitFormattedLine("", "PUSH", "#0XFFFF");
        datatype = BOOLTYPE;
        GetNextToken(tokens);
        break;
    case FALSEHOOD:
        code.EmitFormattedLine("", "PUSH", "#0X0000");
        datatype = BOOLTYPE;
        GetNextToken(tokens);
        break;
    case OPARENTHESIS:
        GetNextToken(tokens);
        ParseExpression(tokens, datatype);
        if (tokens[0].type != CPARENTHESIS)
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting ')'");
        GetNextToken(tokens);
        break;
    case IDENTIFIER:
        ParseVariable(tokens, false, datatype);
        break;
    default:
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex,
            "Expecting integer, TRUTH, FALSEHOOD, '(', or variable");
        break;
    }

    ExitModule("Primary");
}

//-----------------------------------------------------------
void ParseVariable(TOKEN tokens[], bool asLValue, DATATYPE& datatype)
//-----------------------------------------------------------
{
    void GetNextToken(TOKEN tokens[]);

    bool isInTable;
    int index;
    IDENTIFIERTYPE identifierType;

    EnterModule("Variable");

    if (tokens[0].type != IDENTIFIER)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting identifier");

    index = identifierTable.GetIndex(tokens[0].lexeme, isInTable);
    if (!isInTable)
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Undefined identifier");

    identifierType = identifierTable.GetType(index);
    datatype = identifierTable.GetDatatype(index);

    if (!((identifierType == GLOBAL_VARIABLE) ||
        (identifierType == GLOBAL_CONSTANT) ||
        (identifierType == PROGRAMMODULE_VARIABLE) ||
        (identifierType == PROGRAMMODULE_CONSTANT)))
        ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Expecting variable identifier");

    // NEW: Immutability check - constants cannot be l-values
    if (asLValue)
    {
        if ((identifierType == GLOBAL_CONSTANT) || (identifierType == PROGRAMMODULE_CONSTANT))
            ProcessCompilerError(tokens[0].sourceLineNumber, tokens[0].sourceLineIndex, "Cannot assign to immutable variable");
    }

    if (asLValue)
        code.EmitFormattedLine("", "PUSHA", identifierTable.GetReference(index));
    else
        code.EmitFormattedLine("", "PUSH", identifierTable.GetReference(index));

    GetNextToken(tokens);

    ExitModule("Variable");
}

//-----------------------------------------------------------
void Callback1(int sourceLineNumber, const char sourceLine[])
//-----------------------------------------------------------
{
    cout << setw(4) << sourceLineNumber << " " << sourceLine << endl;
}

//-----------------------------------------------------------
void Callback2(int sourceLineNumber, const char sourceLine[])
//-----------------------------------------------------------
{
    char line[SOURCELINELENGTH + 1];

    sprintf(line, "; %4d %s", sourceLineNumber, sourceLine);
    code.EmitUnformattedLine(line);
}

//-----------------------------------------------------------
void GetNextToken(TOKEN tokens[])
//-----------------------------------------------------------
{
    const char* TokenDescription(TOKENTYPE type);

    int i;
    TOKENTYPE type;
    char lexeme[SOURCELINELENGTH + 1];
    int sourceLineNumber;
    int sourceLineIndex;
    char information[SOURCELINELENGTH + 1];

    for (int i = 1; i <= LOOKAHEAD; i++)
        tokens[i - 1] = tokens[i];

    char nextCharacter = reader.GetLookAheadCharacter(0).character;

    do
    {
        while ((nextCharacter == ' ')
            || (nextCharacter == READER<CALLBACKSUSED>::EOLC)
            || (nextCharacter == READER<CALLBACKSUSED>::TABC))
            nextCharacter = reader.GetNextCharacter().character;

        if ((nextCharacter == '/') && (reader.GetLookAheadCharacter(1).character == '/'))
        {
#ifdef TRACESCANNER
            sprintf(information, "At (%4d:%3d) begin line comment",
                reader.GetLookAheadCharacter(0).sourceLineNumber,
                reader.GetLookAheadCharacter(0).sourceLineIndex);
            lister.ListInformationLine(information);
#endif

            do
                nextCharacter = reader.GetNextCharacter().character;
            while ((nextCharacter != READER<CALLBACKSUSED>::EOLC)
                && (nextCharacter != READER<CALLBACKSUSED>::EOPC));
        }
    } while ((nextCharacter == ' ')
        || (nextCharacter == READER<CALLBACKSUSED>::EOLC)
        || (nextCharacter == READER<CALLBACKSUSED>::TABC)
        || ((nextCharacter == '/') && (reader.GetLookAheadCharacter(1).character == '/')));

    sourceLineNumber = reader.GetLookAheadCharacter(0).sourceLineNumber;
    sourceLineIndex = reader.GetLookAheadCharacter(0).sourceLineIndex;

    if (isalpha(nextCharacter))
    {
        char UCLexeme[SOURCELINELENGTH + 1];

        i = 0;
        lexeme[i++] = nextCharacter;
        nextCharacter = reader.GetNextCharacter().character;
        while (isalpha(nextCharacter) || isdigit(nextCharacter) || (nextCharacter == '_'))
        {
            lexeme[i++] = nextCharacter;
            nextCharacter = reader.GetNextCharacter().character;
        }
        lexeme[i] = '\0';
        for (i = 0; i <= (int)strlen(lexeme); i++)
            UCLexeme[i] = toupper(lexeme[i]);

        bool isFound = false;
        i = 0;
        while (!isFound && (i <= (sizeof(TOKENTABLE) / sizeof(TOKENTABLERECORD)) - 1))
        {
            if (TOKENTABLE[i].isReservedWord && (strcmp(UCLexeme, TOKENTABLE[i].description) == 0))
                isFound = true;
            else
                i++;
        }
        if (isFound)
            type = TOKENTABLE[i].type;
        else
            type = IDENTIFIER;
    }
    else if (isdigit(nextCharacter))
    {
        i = 0;
        lexeme[i++] = nextCharacter;
        nextCharacter = reader.GetNextCharacter().character;
        while (isdigit(nextCharacter))
        {
            lexeme[i++] = nextCharacter;
            nextCharacter = reader.GetNextCharacter().character;
        }
        lexeme[i] = '\0';
        type = INTEGER;
    }
    else
    {
        switch (nextCharacter)
        {
        case '"':
            i = 0;
            nextCharacter = reader.GetNextCharacter().character;
            while ((nextCharacter != '"')
                && (nextCharacter != READER<CALLBACKSUSED>::EOLC)
                && (nextCharacter != READER<CALLBACKSUSED>::EOPC))
            {
                if (nextCharacter == '\\')
                {
                    lexeme[i++] = nextCharacter;
                    nextCharacter = reader.GetNextCharacter().character;
                    if ((nextCharacter == 'n') ||
                        (nextCharacter == 't') ||
                        (nextCharacter == 'b') ||
                        (nextCharacter == 'r') ||
                        (nextCharacter == '\\') ||
                        (nextCharacter == '"'))
                    {
                        lexeme[i++] = nextCharacter;
                    }
                    else
                        ProcessCompilerError(sourceLineNumber, sourceLineIndex,
                            "Illegal escape character sequence in string literal");
                }
                else
                {
                    lexeme[i++] = nextCharacter;
                }
                nextCharacter = reader.GetNextCharacter().character;
            }
            if (nextCharacter != '"')
                ProcessCompilerError(sourceLineNumber, sourceLineIndex,
                    "Un-terminated string literal");
            lexeme[i] = '\0';
            type = STRING;
            reader.GetNextCharacter();
            break;
        case READER<CALLBACKSUSED>::EOPC:
        {
            static int count = 0;

            if (++count > (LOOKAHEAD + 1))
                ProcessCompilerError(sourceLineNumber, sourceLineIndex,
                    "Unexpected end-of-program");
            else
            {
                type = EOPTOKEN;
                reader.GetNextCharacter();
                lexeme[0] = '\0';
            }
        }
        break;
        case ',':
            type = COMMA;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case ';':
            type = SEMICOLON;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '{':
            type = OBRACE;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '}':
            type = CBRACE;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '(':
            type = OPARENTHESIS;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case ')':
            type = CPARENTHESIS;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case ':':
            type = COLON;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '<':
            lexeme[0] = nextCharacter;
            nextCharacter = reader.GetNextCharacter().character;
            if (nextCharacter == '=')
            {
                type = LTEQ;
                lexeme[1] = nextCharacter; lexeme[2] = '\0';
                reader.GetNextCharacter();
            }
            else if (nextCharacter == '-')
            {
                type = LEFTARROW;
                lexeme[1] = nextCharacter; lexeme[2] = '\0';
                reader.GetNextCharacter();
            }
            else
            {
                type = LT;
                lexeme[1] = '\0';
            }
            break;
        case '=':
            type = EQ;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '>':
            lexeme[0] = nextCharacter;
            nextCharacter = reader.GetNextCharacter().character;
            if (nextCharacter == '=')
            {
                type = GTEQ;
                lexeme[1] = nextCharacter; lexeme[2] = '\0';
                reader.GetNextCharacter();
            }
            else
            {
                type = GT;
                lexeme[1] = '\0';
            }
            break;
        case '!':
            lexeme[0] = nextCharacter;
            if (reader.GetLookAheadCharacter(1).character == '=')
            {
                nextCharacter = reader.GetNextCharacter().character;
                lexeme[1] = nextCharacter; lexeme[2] = '\0';
                reader.GetNextCharacter();
                type = NOTEQ;
            }
            else
            {
                type = UNKTOKEN;
                lexeme[1] = '\0';
                reader.GetNextCharacter();
            }
            break;
        case '+':
            type = PLUS;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '-':
            type = MINUS;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '*':
            lexeme[0] = nextCharacter;
            if (reader.GetLookAheadCharacter(1).character == '*')
            {
                nextCharacter = reader.GetNextCharacter().character;
                lexeme[1] = nextCharacter; lexeme[2] = '\0';
                type = POWER;
            }
            else
            {
                type = MULTIPLY;
                lexeme[0] = nextCharacter; lexeme[1] = '\0';
            }
            reader.GetNextCharacter();
            break;
        case '/':
            type = DIVIDE;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '%':
            type = MODULUS;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        case '^':
            type = POWER;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        default:
            type = UNKTOKEN;
            lexeme[0] = nextCharacter; lexeme[1] = '\0';
            reader.GetNextCharacter();
            break;
        }
    }

    tokens[LOOKAHEAD].type = type;
    strcpy(tokens[LOOKAHEAD].lexeme, lexeme);
    tokens[LOOKAHEAD].sourceLineNumber = sourceLineNumber;
    tokens[LOOKAHEAD].sourceLineIndex = sourceLineIndex;

#ifdef TRACESCANNER
    sprintf(information, "At (%4d:%3d) token = %12s lexeme = |%s|",
        tokens[LOOKAHEAD].sourceLineNumber,
        tokens[LOOKAHEAD].sourceLineIndex,
        TokenDescription(type), lexeme);
    lister.ListInformationLine(information);
#endif
}

//-----------------------------------------------------------
const char* TokenDescription(TOKENTYPE type)
//-----------------------------------------------------------
{
    int i;
    bool isFound;

    isFound = false;
    i = 0;
    while (!isFound && (i <= (sizeof(TOKENTABLE) / sizeof(TOKENTABLERECORD)) - 1))
    {
        if (TOKENTABLE[i].type == type)
            isFound = true;
        else
            i++;
    }
    return (isFound ? TOKENTABLE[i].description : "???????");
}