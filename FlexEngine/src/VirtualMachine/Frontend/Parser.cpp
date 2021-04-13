#include "stdafx.hpp"

#include "VirtualMachine/Frontend/Parser.hpp"

#include "StringBuilder.hpp"
#include "VirtualMachine/Backend/VariableContainer.hpp"
#include "VirtualMachine/Backend/VirtualMachine.hpp"
#include "VirtualMachine/Frontend/Lexer.hpp"
#include "VirtualMachine/Frontend/Token.hpp"

namespace flex
{
	namespace AST
	{
		static Identifier* g_InvalidIdentifier = (Identifier*)0xFFFFFFFFFFFFFFFF;

		const char* UnaryOperatorTypeToString(UnaryOperatorType opType)
		{
			return g_UnaryOperatorTypeStrings[(u32)opType];
		}

		UnaryOperatorType TokenKindToUnaryOperatorType(TokenKind tokenKind)
		{
			switch (tokenKind)
			{
			case TokenKind::MINUS:			return UnaryOperatorType::NEGATE;
			case TokenKind::BANG:			return UnaryOperatorType::NOT;
			case TokenKind::TILDE:			return UnaryOperatorType::BIN_INVERT;
			default: return UnaryOperatorType::_NONE;
			}
		}

		const char* BinaryOperatorTypeToString(BinaryOperatorType opType)
		{
			return g_BinaryOperatorTypeStrings[(u32)opType];
		}

		bool IsCompoundAssignment(BinaryOperatorType operatorType)
		{
			switch (operatorType)
			{
			case BinaryOperatorType::ADD_ASSIGN:
			case BinaryOperatorType::SUB_ASSIGN:
			case BinaryOperatorType::MUL_ASSIGN:
			case BinaryOperatorType::DIV_ASSIGN:
			case BinaryOperatorType::MOD_ASSIGN:
			case BinaryOperatorType::BIN_AND_ASSIGN:
			case BinaryOperatorType::BIN_OR_ASSIGN:
			case BinaryOperatorType::BIN_XOR_ASSIGN:
				return true;
			default:
				return false;
			}
		}

		BinaryOperatorType GetNonCompoundType(BinaryOperatorType operatorType)
		{

			switch (operatorType)
			{
			case BinaryOperatorType::ADD_ASSIGN:		return BinaryOperatorType::ADD;
			case BinaryOperatorType::SUB_ASSIGN:		return BinaryOperatorType::SUB;
			case BinaryOperatorType::MUL_ASSIGN:		return BinaryOperatorType::MUL;
			case BinaryOperatorType::DIV_ASSIGN:		return BinaryOperatorType::DIV;
			case BinaryOperatorType::MOD_ASSIGN:		return BinaryOperatorType::MOD;
			case BinaryOperatorType::BIN_AND_ASSIGN:	return BinaryOperatorType::BIN_AND;
			case BinaryOperatorType::BIN_OR_ASSIGN:		return BinaryOperatorType::BIN_OR;
			case BinaryOperatorType::BIN_XOR_ASSIGN:	return BinaryOperatorType::BIN_XOR;
			default: return BinaryOperatorType::_NONE;
			}

		}

		BinaryOperatorType TokenKindToBinaryOperatorType(TokenKind tokenKind)
		{
			switch (tokenKind)
			{
			case TokenKind::EQUALS:				return BinaryOperatorType::ASSIGN;
			case TokenKind::PLUS:				return BinaryOperatorType::ADD;
			case TokenKind::PLUS_EQUALS:		return BinaryOperatorType::ADD_ASSIGN;
			case TokenKind::MINUS:				return BinaryOperatorType::SUB;
			case TokenKind::MINUS_EQUALS:		return BinaryOperatorType::SUB_ASSIGN;
			case TokenKind::STAR:				return BinaryOperatorType::MUL;
			case TokenKind::STAR_EQUALS:		return BinaryOperatorType::MUL_ASSIGN;
			case TokenKind::FOR_SLASH:			return BinaryOperatorType::DIV;
			case TokenKind::FOR_SLASH_EQUALS:	return BinaryOperatorType::DIV_ASSIGN;
			case TokenKind::PERCENT:			return BinaryOperatorType::MOD;
			case TokenKind::PERCENT_EQUALS:		return BinaryOperatorType::MOD_ASSIGN;
			case TokenKind::BINARY_AND:			return BinaryOperatorType::BIN_AND;
			case TokenKind::BINARY_AND_EQUALS:	return BinaryOperatorType::BIN_AND_ASSIGN;
			case TokenKind::BINARY_OR:			return BinaryOperatorType::BIN_OR;
			case TokenKind::BINARY_OR_EQUALS:	return BinaryOperatorType::BIN_OR_ASSIGN;
			case TokenKind::BINARY_XOR:			return BinaryOperatorType::BIN_XOR;
			case TokenKind::BINARY_XOR_EQUALS:	return BinaryOperatorType::BIN_XOR_ASSIGN;
			case TokenKind::EQUAL_EQUAL:		return BinaryOperatorType::EQUAL_TEST;
			case TokenKind::NOT_EQUAL:			return BinaryOperatorType::NOT_EQUAL_TEST;
			case TokenKind::GREATER:			return BinaryOperatorType::GREATER_TEST;
			case TokenKind::GREATER_EQUAL:		return BinaryOperatorType::GREATER_EQUAL_TEST;
			case TokenKind::LESS:				return BinaryOperatorType::LESS_TEST;
			case TokenKind::LESS_EQUAL:			return BinaryOperatorType::LESS_EQUAL_TEST;
			case TokenKind::BOOLEAN_AND:		return BinaryOperatorType::BOOLEAN_AND;
			case TokenKind::BOOLEAN_OR:			return BinaryOperatorType::BOOLEAN_OR;
			default: return BinaryOperatorType::_NONE;
			}
		}

		i32 GetBinaryOperatorPrecedence(TokenKind tokenKind)
		{
			switch (tokenKind)
			{
			case TokenKind::STAR:
			case TokenKind::FOR_SLASH:
			case TokenKind::PERCENT:
				return 1100;
			case TokenKind::PLUS:
			case TokenKind::MINUS:
				return 1000;
			case TokenKind::GREATER:
			case TokenKind::GREATER_EQUAL:
			case TokenKind::LESS:
			case TokenKind::LESS_EQUAL:
				return 900;
			case TokenKind::EQUAL_EQUAL:
			case TokenKind::NOT_EQUAL:
				return 800;
			case TokenKind::BINARY_AND:
				return 700;
			case TokenKind::BINARY_XOR:
				return 600;
			case TokenKind::BINARY_OR:
				return 500;
			case TokenKind::BOOLEAN_AND:
				return 400;
			case TokenKind::BOOLEAN_OR:
				return 300;
			case TokenKind::QUESTION:
				return 250;
			case TokenKind::EQUALS:
			case TokenKind::PLUS_EQUALS:
			case TokenKind::MINUS_EQUALS:
			case TokenKind::STAR_EQUALS:
			case TokenKind::FOR_SLASH_EQUALS:
			case TokenKind::PERCENT_EQUALS:
				return 200;
			default:
				return -1;
			}
		}

		bool BinaryOperatorTypeIsTest(BinaryOperatorType operatorType)
		{
			return operatorType == BinaryOperatorType::EQUAL_TEST ||
				operatorType == BinaryOperatorType::NOT_EQUAL_TEST ||
				operatorType == BinaryOperatorType::GREATER_TEST ||
				operatorType == BinaryOperatorType::GREATER_EQUAL_TEST ||
				operatorType == BinaryOperatorType::LESS_TEST ||
				operatorType == BinaryOperatorType::LESS_EQUAL_TEST ||
				operatorType == BinaryOperatorType::BOOLEAN_AND ||
				operatorType == BinaryOperatorType::BOOLEAN_OR;
		}

		std::string TypeNameToString(TypeName typeName)
		{
			return g_TypeNameStrings[(u32)typeName];
		}

		TypeName TokenKindToTypeName(TokenKind tokenKind)
		{
			switch (tokenKind)
			{
			case TokenKind::INT_KEYWORD:	return TypeName::INT;
			case TokenKind::FLOAT_KEYWORD:	return TypeName::FLOAT;
			case TokenKind::BOOL_KEYWORD:	return TypeName::BOOL;
			case TokenKind::STRING_KEYWORD:	return TypeName::STRING;
			case TokenKind::CHAR_KEYWORD:	return TypeName::CHAR;
			case TokenKind::VOID_KEYWORD:	return TypeName::VOID;
			default: return TypeName::_NONE;
			}
		}

		TypeName TypeNameToListVariant(TypeName baseType)
		{
			switch (baseType)
			{
			case TypeName::INT:		return TypeName::INT_LIST;
			case TypeName::FLOAT:	return TypeName::FLOAT_LIST;
			case TypeName::BOOL:	return TypeName::BOOL_LIST;
			case TypeName::STRING:	return TypeName::STRING_LIST;
			case TypeName::CHAR:	return TypeName::CHAR_LIST;
			default: return TypeName::_NONE;
			}
		}

		TypeName ValueTypeToTypeName(ValueType valueType)
		{
			switch (valueType)
			{
			case ValueType::INT_RAW:	return TypeName::INT;
			case ValueType::FLOAT_RAW:	return TypeName::FLOAT;
			case ValueType::BOOL_RAW:	return TypeName::BOOL;
			default: return TypeName::_NONE;
			}
		}

		ValueType TypeNameToValueType(TypeName typeName)
		{
			switch (typeName)
			{
			case TypeName::INT:		return ValueType::INT_RAW;
			case TypeName::FLOAT:	return ValueType::FLOAT_RAW;
			case TypeName::BOOL:	return ValueType::BOOL_RAW;
			default: return ValueType::_NONE;
			}
		}

		bool CanBeReduced(StatementType statementType)
		{
			return statementType == StatementType::UNARY_OPERATION ||
				statementType == StatementType::BINARY_OPERATION ||
				statementType == StatementType::TERNARY_OPERATION ||
				statementType == StatementType::FUNC_CALL ||
				statementType == StatementType::INDEX_OPERATION;
		}

		bool IsLiteral(StatementType statementType)
		{
			return statementType == StatementType::INT_LIT ||
				statementType == StatementType::FLOAT_LIT ||
				statementType == StatementType::BOOL_LIT ||
				statementType == StatementType::STRING_LIT ||
				statementType == StatementType::CHAR_LIT;
		}

		bool IsExpression(StatementType statementType)
		{
			return statementType == StatementType::BREAK ||
				statementType == StatementType::YIELD ||
				statementType == StatementType::RETURN ||
				statementType == StatementType::IDENTIFIER ||
				statementType == StatementType::INT_LIT ||
				statementType == StatementType::FLOAT_LIT ||
				statementType == StatementType::BOOL_LIT ||
				statementType == StatementType::STRING_LIT ||
				statementType == StatementType::CHAR_LIT ||
				statementType == StatementType::INDEX_OPERATION ||
				statementType == StatementType::UNARY_OPERATION ||
				statementType == StatementType::BINARY_OPERATION ||
				statementType == StatementType::TERNARY_OPERATION ||
				statementType == StatementType::FUNC_CALL;
		}

		bool IsSimple(StatementType statementType)
		{
			return IsLiteral(statementType) ||
				statementType == StatementType::IDENTIFIER ||
				statementType == StatementType::FUNC_ARG;
		}

		Statement::Statement(const Span& span, StatementType statementType) :
			span(span),
			statementType(statementType)
		{
		}

		Identifier* Statement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			FLEX_UNUSED(parser);
			FLEX_UNUSED(tmpStatements);
			return nullptr;
		}

		void Statement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			FLEX_UNUSED(varContainer);
			FLEX_UNUSED(diagnosticContainer);
		}

		StatementBlock::StatementBlock(const Span& span, const std::vector<Statement*>& statements) :
			Statement(span, StatementType::STATEMENT_BLOCK),
			statements(statements)
		{
		}

		StatementBlock::~StatementBlock()
		{
			for (u32 i = 0; i < (u32)statements.size(); ++i)
			{
				delete statements[i];
			}
		}

		std::string StatementBlock::ToString() const
		{
			StringBuilder stringBuilder;

			// Don't print brackets for root block
			bool bBrackets = (span.low > 0);

			if (bBrackets)
			{
				stringBuilder.Append("{\n");
			}
			for (Statement* statement : statements)
			{
				stringBuilder.Append(statement->ToString());
				stringBuilder.Append("\n");
			}
			if (bBrackets)
			{
				stringBuilder.Append("}");
			}

			return stringBuilder.ToString();
		}

		Identifier* StatementBlock::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			for (u32 i = 0; i < (u32)statements.size(); /* */)
			{
				Identifier* tmpIdent = statements[i]->RewriteCompoundStatements(parser, tmpStatements);

				if (tmpIdent != nullptr)
				{
					if (tmpIdent == g_InvalidIdentifier)
					{
						delete statements[i];
						statements.erase(statements.begin() + i);
					}
					else
					{
						statements[i] = tmpIdent;
					}
				}

				if (tmpStatements.empty())
				{
					++i;
				}
				else
				{
					statements.insert(statements.begin() + i, tmpStatements.begin(), tmpStatements.end());
					i += (u32)tmpStatements.size();
					tmpStatements.clear();
				}
			}

			return nullptr;
		}

		void StatementBlock::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			varContainer->PushFrame();

			bool bPrevWriteFlagSet = varContainer->GetWriteFlag();

			do
			{
				varContainer->ClearVarsInFrame();
				varContainer->ClearWriteFlag();

				for (u32 i = 0; i < (u32)statements.size(); ++i)
				{
					statements[i]->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
				}

			} while (varContainer->GetWriteFlag());

			if (bPrevWriteFlagSet)
			{
				varContainer->SetWriteFlag();
			}

			varContainer->PopFrame();
		}

		IfStatement::~IfStatement()
		{
			delete condition;
			delete then;
			delete otherwise;
		}

		std::string IfStatement::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("if (");
			stringBuilder.Append(condition->ToString());
			stringBuilder.Append(")\n");
			stringBuilder.Append(then->ToString());
			if (otherwise != nullptr)
			{
				if (dynamic_cast<IfStatement*>(otherwise) != nullptr)
				{
					stringBuilder.Append("\nelif (");
				}
				else
				{
					stringBuilder.Append("\nelse\n");
				}
				stringBuilder.Append(otherwise->ToString());
			}

			return stringBuilder.ToString();
		}

		Identifier* IfStatement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Identifier* newCondition = condition->RewriteCompoundStatements(parser, tmpStatements);
			if (newCondition != nullptr)
			{
				condition = newCondition;
			}

			// Don't pass outer tmpStatements in since they will be then inserted into child statement blocks
			std::vector<Statement*> tmpStatementsInner;
			Identifier* newThen = then->RewriteCompoundStatements(parser, tmpStatementsInner);
			if (newThen != nullptr)
			{
				then = newThen;
			}

			if (otherwise != nullptr)
			{
				Identifier* newOtherwise = otherwise->RewriteCompoundStatements(parser, tmpStatementsInner);
				if (newOtherwise != nullptr)
				{
					otherwise = newOtherwise;
				}
			}

			tmpStatements.insert(tmpStatements.end(), tmpStatementsInner.begin(), tmpStatementsInner.end());

			return nullptr;
		}

		void IfStatement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			varContainer->PushFrame();

			condition->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			if (condition->typeName != TypeName::BOOL)
			{
				diagnosticContainer->AddDiagnostic(condition->span, "Condition statement must evaluate to a boolean value (not " + std::string(TypeNameToString(condition->typeName)) + ")");
			}

			then->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

			if (otherwise != nullptr)
			{
				otherwise->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}

			varContainer->PopFrame();
		}

		ForStatement::~ForStatement()
		{
			delete setup;
			delete condition;
			delete update;
			delete body;
		}

		std::string ForStatement::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("for (");
			if (setup != nullptr)
			{
				stringBuilder.Append(setup->ToString());
			}
			stringBuilder.Append(" ");
			if (condition != nullptr)
			{
				stringBuilder.Append(condition->ToString());
			}
			stringBuilder.Append("; ");
			if (update != nullptr)
			{
				stringBuilder.Append(update->ToString());
			}
			stringBuilder.Append(")\n");
			stringBuilder.Append(body->ToString());

			return stringBuilder.ToString();
		}

		Identifier* ForStatement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Identifier* newSetup = setup->RewriteCompoundStatements(parser, tmpStatements);
			if (newSetup != nullptr)
			{
				setup = newSetup;
			}

			std::vector<Statement*> tmpStatementsInner;
			Identifier* newCondition = condition->RewriteCompoundStatements(parser, tmpStatementsInner);
			if (newCondition != nullptr)
			{
				delete condition;
				condition = new StatementBlock(span, tmpStatementsInner);
				if (newCondition != g_InvalidIdentifier)
				{
					delete newCondition; // TODO: Prevent creation of this?
				}
			}

			std::vector<Statement*> tmpStatementsUpdate;
			Identifier* newUpdate = update->RewriteCompoundStatements(parser, tmpStatementsUpdate);
			if (newUpdate != nullptr)
			{
				delete update;
				update = new StatementBlock(span, tmpStatementsUpdate);
				if (newUpdate != g_InvalidIdentifier)
				{
					delete newUpdate; // TODO: Prevent creation of this?
				}
			}

			std::vector<Statement*> tmpStatementsBody;
			Identifier* newBody = body->RewriteCompoundStatements(parser, tmpStatementsBody);
			if (newBody != nullptr)
			{
				body = new StatementBlock(span, tmpStatementsBody);
				delete newBody; // TODO: Prevent creation of this?
			}

			return nullptr;
		}

		void ForStatement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			varContainer->PushFrame();

			if (setup != nullptr)
			{
				setup->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}

			if (condition != nullptr)
			{
				condition->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}

			if (update != nullptr)
			{
				update->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}

			body->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

			varContainer->PopFrame();
		}

		WhileStatement::~WhileStatement()
		{
			delete condition;
			delete body;
		}

		std::string WhileStatement::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("while (");
			stringBuilder.Append(condition->ToString());
			stringBuilder.Append(")\n");
			stringBuilder.Append(body->ToString());

			return stringBuilder.ToString();
		}

		Identifier* WhileStatement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Identifier* conditionTempIdent = condition->RewriteCompoundStatements(parser, tmpStatements);
			if (conditionTempIdent != nullptr)
			{
				condition = conditionTempIdent;
			}

			std::vector<Statement*> tmpStatementsInner;
			Identifier* bodyTempIdent = body->RewriteCompoundStatements(parser, tmpStatementsInner);
			if (bodyTempIdent != nullptr)
			{
				body = bodyTempIdent;
			}

			tmpStatements.insert(tmpStatements.end(), tmpStatementsInner.begin(), tmpStatementsInner.end());

			return nullptr;
		}

		void WhileStatement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			varContainer->PushFrame();

			condition->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			if (condition->typeName != TypeName::BOOL)
			{
				diagnosticContainer->AddDiagnostic(condition->span, "Condition statement must evaluate to a boolean value (not " + std::string(TypeNameToString(condition->typeName)) + ")");
			}

			body->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

			varContainer->PopFrame();
		}

		DoWhileStatement::~DoWhileStatement()
		{
			delete condition;
			delete body;
		}

		std::string DoWhileStatement::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("do\n");
			stringBuilder.Append(body->ToString());
			stringBuilder.Append(" while (");
			stringBuilder.Append(condition->ToString());
			stringBuilder.Append(");");

			return stringBuilder.ToString();
		}

		Identifier* DoWhileStatement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Identifier* conditionTempIdent = condition->RewriteCompoundStatements(parser, tmpStatements);
			if (conditionTempIdent != nullptr)
			{
				condition = conditionTempIdent;
			}

			std::vector<Statement*> tmpStatementsInner;
			Identifier* bodyTempIdent = body->RewriteCompoundStatements(parser, tmpStatementsInner);
			if (bodyTempIdent != nullptr)
			{
				body = bodyTempIdent;
			}

			tmpStatements.insert(tmpStatements.end(), tmpStatementsInner.begin(), tmpStatementsInner.end());

			return nullptr;
		}

		void DoWhileStatement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			varContainer->PushFrame();

			condition->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			if (condition->typeName != TypeName::BOOL)
			{
				diagnosticContainer->AddDiagnostic(condition->span, "Condition statement must evaluate to a boolean value (not " + std::string(TypeNameToString(condition->typeName)) + ")");
			}

			body->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

			varContainer->PopFrame();
		}

		FunctionDeclaration::~FunctionDeclaration()
		{
			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				delete arguments[i];
			}

			delete body;
		}

		std::string FunctionDeclaration::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("func ");
			stringBuilder.Append(name);
			stringBuilder.Append("(");
			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				stringBuilder.Append(arguments[i]->ToString());
				if (i < (u32)arguments.size() - 1)
				{
					stringBuilder.Append(", ");
				}
			}
			stringBuilder.Append(") -> ");
			stringBuilder.Append(TypeNameToString(returnType));
			stringBuilder.Append(" ");
			stringBuilder.Append(body->ToString());

			return stringBuilder.ToString();
		}

		Identifier* FunctionDeclaration::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			body->RewriteCompoundStatements(parser, tmpStatements);
			return nullptr;
		}

		void FunctionDeclaration::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			varContainer->DeclareFunction(this);

			varContainer->PushFrame();

			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				arguments[i]->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}

			body->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

			varContainer->PopFrame();
		}

		YieldStatement::~YieldStatement()
		{
			delete yieldValue;
		}

		std::string YieldStatement::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("yield");

			if (yieldValue != nullptr)
			{
				stringBuilder.Append(" ");
				stringBuilder.Append(yieldValue->ToString());
			}

			stringBuilder.Append(";");

			return stringBuilder.ToString();
		}

		Identifier* YieldStatement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			if (yieldValue != nullptr)
			{
				Identifier* yieldValueTempIdent = yieldValue->RewriteCompoundStatements(parser, tmpStatements);
				if (yieldValueTempIdent != nullptr)
				{
					yieldValue = yieldValueTempIdent;
				}
			}

			return nullptr;
		}

		void YieldStatement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			if (yieldValue != nullptr)
			{
				yieldValue->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

				if (yieldValue->typeName != TypeName::_NONE)
				{
					diagnosticContainer->AddDiagnostic(yieldValue->span, "Yield value must be non-void");
				}
			}
		}

		ReturnStatement::~ReturnStatement()
		{
			delete returnValue;
		}

		std::string ReturnStatement::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("return");

			if (returnValue != nullptr)
			{
				stringBuilder.Append(" ");
				stringBuilder.Append(returnValue->ToString());
			}

			stringBuilder.Append(";");

			return stringBuilder.ToString();
		}

		Identifier* ReturnStatement::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			if (returnValue != nullptr)
			{
				Identifier* returnValueTempIdent = returnValue->RewriteCompoundStatements(parser, tmpStatements);
				if (returnValueTempIdent != nullptr)
				{
					returnValue = returnValueTempIdent;
				}
			}

			return nullptr;
		}

		void ReturnStatement::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			if (returnValue != nullptr)
			{
				returnValue->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

				if (returnValue->typeName == TypeName::_NONE)
				{
					diagnosticContainer->AddDiagnostic(returnValue->span, "Return value must be non-void");
				}
			}
		}

		Declaration::~Declaration()
		{
			delete initializer;
		}

		std::string Declaration::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append(TypeNameToString(typeName));
			stringBuilder.Append(" ");
			stringBuilder.Append(identifierStr);

			if (!bIsFunctionArgDefinition)
			{
				stringBuilder.Append(" = ");
				stringBuilder.Append(initializer->ToString());
				stringBuilder.Append(";");
			}

			return stringBuilder.ToString();
		}

		Identifier* Declaration::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			if (!bIsFunctionArgDefinition)
			{
				Identifier* initializerTempIdent = initializer->RewriteCompoundStatements(parser, tmpStatements);
				if (initializerTempIdent != nullptr)
				{
					initializer = initializerTempIdent;
				}
			}

			return nullptr;
		}

		void Declaration::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			TypeName testTypeName;
			if (varContainer->GetTypeName(identifierStr, testTypeName))
			{
				if (testTypeName != typeName)
				{
					std::string diagnosticStr = "Multiple definitions found of '" + identifierStr + "' with different types (" + std::string(TypeNameToString(typeName)) + " vs. " + std::string(TypeNameToString(testTypeName)) + ")";
					diagnosticContainer->AddDiagnostic(initializer->span, diagnosticStr);
				}
				else
				{
					std::string diagnosticStr = "Multiple definitions found of '" + identifierStr;
					diagnosticContainer->AddDiagnostic(initializer->span, diagnosticStr);
				}

				return;
			}

			varContainer->DeclareVariable(this);

			if (!bIsFunctionArgDefinition)
			{
				initializer->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

				if (!varContainer->SetVariableType(identifierStr, initializer->typeName))
				{
					assert(false);
					//std::string diagnosticStr = "Mismatched types (" + std::string(TypeNameToString(initializer->typeName)) + " vs. " + std::string(TypeNameToString(varTypeName)) + ")";
					//diagnosticContainer->AddDiagnostic(initializer->span, diagnosticStr);
				}
			}
		}

		void Identifier::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			TypeName identTypeName;
			if (varContainer->GetTypeName(identifierStr, identTypeName))
			{
				if (typeName != identTypeName)
				{
					typeName = identTypeName;
					varContainer->SetWriteFlag();
				}
			}
			else
			{
				std::string diagnosticStr = "Identifier \"" + identifierStr + "\" not found";
				diagnosticContainer->AddDiagnostic(span, diagnosticStr);
			}
		}

		Assignment::~Assignment()
		{
			delete rhs;
		}

		std::string Assignment::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append(lhs);
			stringBuilder.Append(" = ");
			stringBuilder.Append(rhs->ToString());
			stringBuilder.Append(";");

			return stringBuilder.ToString();
		}

		Identifier* Assignment::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Identifier* rhsTempIdent = rhs->RewriteCompoundStatements(parser, tmpStatements);
			if (rhsTempIdent != nullptr)
			{
				rhs = rhsTempIdent;
			}

			return nullptr;
		}

		void Assignment::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			rhs->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
		}

		CompoundAssignment::~CompoundAssignment()
		{
			delete rhs;
		}

		std::string CompoundAssignment::ToString() const
		{

			StringBuilder stringBuilder;

			stringBuilder.Append(lhs);
			stringBuilder.Append(" ");
			stringBuilder.Append(BinaryOperatorTypeToString(operatorType));
			stringBuilder.Append(" ");
			stringBuilder.Append(rhs->ToString());
			stringBuilder.Append(";");

			return stringBuilder.ToString();
		}

		Identifier* CompoundAssignment::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			assert(IsCompoundAssignment(operatorType));

			Identifier* rhsTempIdent = rhs->RewriteCompoundStatements(parser, tmpStatements);
			if (rhsTempIdent != nullptr)
			{
				rhs = rhsTempIdent;
			}

			Span newSpan(span.low, span.high);
			newSpan.source = Span::Source::GENERATED;

			// e.g.
			// a += b
			//
			// becomes:
			//
			// int tmp = a + b;
			// a = tmp;

			BinaryOperation* simpleBinOp = new BinaryOperation(newSpan, GetNonCompoundType(operatorType), new Identifier(newSpan, lhs), rhs);
			Declaration* tempResult = new Declaration(span, parser->NextTempIdentifier(), simpleBinOp, typeName);

			tmpStatements.push_back(tempResult);

			Assignment* newAssignment = new Assignment(span, lhs, new Identifier(span, tempResult->identifierStr));
			tmpStatements.push_back(newAssignment);

			// Clear out members to prepare for our deletion
			lhs = "";
			rhs = nullptr;
			return g_InvalidIdentifier;
		}

		void CompoundAssignment::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			FLEX_UNUSED(varContainer);
			FLEX_UNUSED(diagnosticContainer);
			// Type propagation should happen after complex types are eliminated (in RewriteCompoundStatement calls)
			assert(false);
		}

		ListInitializer::~ListInitializer()
		{
			for (u32 i = 0; i < (u32)listValues.size(); ++i)
			{
				delete listValues[i];
			}
		}

		std::string ListInitializer::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("{ ");
			for (u32 i = 0; i < (u32)listValues.size(); ++i)
			{
				stringBuilder.Append(listValues[i]->ToString());
				if (i < (u32)listValues.size() - 1)
				{
					stringBuilder.Append(", ");
				}
			}
			stringBuilder.Append(" }");

			return stringBuilder.ToString();
		}

		Identifier* ListInitializer::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			for (u32 i = 0; i < (u32)listValues.size(); ++i)
			{
				Identifier* listValueTempIdent = listValues[i]->RewriteCompoundStatements(parser, tmpStatements);
				if (listValueTempIdent != nullptr)
				{
					listValues[i] = listValueTempIdent;
				}
			}

			return nullptr;
		}

		void ListInitializer::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			for (u32 i = 0; i < (u32)listValues.size(); ++i)
			{
				listValues[i]->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}
		}

		IndexOperation::~IndexOperation()
		{
			delete indexExpression;
		}

		std::string IndexOperation::ToString() const
		{
			StringBuilder stringBuilder;
			stringBuilder.Append(container);
			stringBuilder.Append("[");
			stringBuilder.Append(indexExpression->ToString());
			stringBuilder.Append("]");
			return stringBuilder.ToString();
		}

		Identifier* IndexOperation::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Identifier* indexExpressionTempIdent = indexExpression->RewriteCompoundStatements(parser, tmpStatements);
			if (indexExpressionTempIdent != nullptr)
			{
				indexExpression = indexExpressionTempIdent;
			}

			Declaration* tmpDecl = new Declaration(span, parser->NextTempIdentifier(), this, typeName);
			tmpStatements.push_back(tmpDecl);

			return new Identifier(span, tmpDecl->identifierStr);
		}

		void IndexOperation::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			indexExpression->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
		}

		UnaryOperation::~UnaryOperation()
		{
			delete expression;
		}

		std::string UnaryOperation::ToString() const
		{
			StringBuilder stringBuilder;

			switch (operatorType)
			{
			case UnaryOperatorType::NEGATE:
				stringBuilder.Append('-');
				break;
			case UnaryOperatorType::NOT:
				stringBuilder.Append('!');
				break;
			case UnaryOperatorType::BIN_INVERT:
				stringBuilder.Append('~');
				break;
			default:
				assert(false);
			}

			stringBuilder.Append(expression->ToString());

			return stringBuilder.ToString();
		}

		Identifier* UnaryOperation::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Span newSpan(span.low, span.high);
			newSpan.source = Span::Source::GENERATED;

			Identifier* expressionTempIdent = expression->RewriteCompoundStatements(parser, tmpStatements);
			if (expressionTempIdent != nullptr)
			{
				expression = expressionTempIdent;
			}

			Declaration* tmpDecl = new Declaration(span, parser->NextTempIdentifier(), this, typeName);
			tmpStatements.push_back(tmpDecl);

			return new Identifier(span, tmpDecl->identifierStr);
		}

		void UnaryOperation::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			expression->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
		}

		BinaryOperation::~BinaryOperation()
		{
			delete lhs;
			delete rhs;
		}

		std::string BinaryOperation::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("(");
			stringBuilder.Append(lhs->ToString());
			stringBuilder.Append(" ");
			stringBuilder.Append(BinaryOperatorTypeToString(operatorType));
			stringBuilder.Append(" ");
			stringBuilder.Append(rhs->ToString());
			stringBuilder.Append(")");

			return stringBuilder.ToString();
		}

		Identifier* BinaryOperation::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Span newSpan(span.low, span.high);
			newSpan.source = Span::Source::GENERATED;

			Identifier* lhsTempIdent = lhs->RewriteCompoundStatements(parser, tmpStatements);
			if (lhsTempIdent != nullptr)
			{
				lhs = lhsTempIdent;
			}

			Identifier* rhsTempIdent = rhs->RewriteCompoundStatements(parser, tmpStatements);
			if (rhsTempIdent != nullptr)
			{
				rhs = rhsTempIdent;
			}

			Declaration* tmpDecl = new Declaration(span, parser->NextTempIdentifier(), this, typeName);
			tmpStatements.push_back(tmpDecl);

			return new Identifier(span, tmpDecl->identifierStr);
		}

		void BinaryOperation::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			lhs->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			rhs->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			if (lhs->typeName != rhs->typeName)
			{

				std::string diagnosticStr = "Mismatched types (" + std::string(TypeNameToString(lhs->typeName)) + " vs. " + std::string(TypeNameToString(rhs->typeName)) + ")";
				diagnosticContainer->AddDiagnostic(lhs->span.Extend(rhs->span), diagnosticStr);
			}
			else
			{
				TypeName targetTypeName = lhs->typeName;
				if (BinaryOperatorTypeIsTest(operatorType))
				{
					targetTypeName = TypeName::BOOL;
				}

				if (typeName != targetTypeName)
				{
					typeName = targetTypeName;
					varContainer->SetWriteFlag();
				}
			}
		}

		TernaryOperation::~TernaryOperation()
		{
			delete condition;
			delete ifTrue;
			delete ifFalse;
		}

		std::string TernaryOperation::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append(condition->ToString());
			stringBuilder.Append(" ? ");
			stringBuilder.Append(ifTrue->ToString());
			stringBuilder.Append(" : ");
			stringBuilder.Append(ifFalse->ToString());

			return stringBuilder.ToString();
		}

		Identifier* TernaryOperation::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			Span newSpan(span.low, span.high);
			newSpan.source = Span::Source::GENERATED;

			Identifier* conditionTempIdent = condition->RewriteCompoundStatements(parser, tmpStatements);
			if (conditionTempIdent != nullptr)
			{
				condition = conditionTempIdent;
			}

			Identifier* ifTrueTempIdent = ifTrue->RewriteCompoundStatements(parser, tmpStatements);
			if (ifTrueTempIdent != nullptr)
			{
				ifTrue = ifTrueTempIdent;
			}

			Identifier* ifFalseTempIdent = ifFalse->RewriteCompoundStatements(parser, tmpStatements);
			if (ifFalseTempIdent != nullptr)
			{
				ifFalse = ifFalseTempIdent;
			}

			Declaration* tmpDecl = new Declaration(span, parser->NextTempIdentifier(), this, typeName);
			tmpStatements.push_back(tmpDecl);

			return new Identifier(span, tmpDecl->identifierStr);
		}

		void TernaryOperation::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			condition->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			ifTrue->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			ifFalse->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);

			if (ifTrue->typeName != ifFalse->typeName)
			{
				std::string diagnosticStr = "Mismatched types (" + std::string(TypeNameToString(ifTrue->typeName)) + " vs. " + std::string(TypeNameToString(ifFalse->typeName)) + ")";
				diagnosticContainer->AddDiagnostic(ifTrue->span.Extend(ifFalse->span), diagnosticStr);
			}
			else
			{
				if (typeName != ifTrue->typeName)
				{
					typeName = ifTrue->typeName;
					varContainer->SetWriteFlag();
				}
			}
		}

		FunctionCall::~FunctionCall()
		{
			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				delete arguments[i];
			}
		}

		std::string FunctionCall::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append(target);
			stringBuilder.Append("(");
			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				stringBuilder.Append(arguments[i]->ToString());

				if (i < (u32)arguments.size() - 1)
				{
					stringBuilder.Append(", ");
				}

			}
			stringBuilder.Append(")");

			return stringBuilder.ToString();
		}

		Identifier* FunctionCall::RewriteCompoundStatements(Parser* parser, std::vector<Statement*>& tmpStatements)
		{
			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				Identifier* argTempIdent = arguments[i]->RewriteCompoundStatements(parser, tmpStatements);
				if (argTempIdent != nullptr)
				{
					arguments[i] = argTempIdent;
				}
			}

			// TODO: Determine called function's return type to know if we need to store the result or not.

			Declaration* tmpDecl = new Declaration(span, parser->NextTempIdentifier(), this, typeName);
			tmpStatements.push_back(tmpDecl);

			return new Identifier(span, tmpDecl->identifierStr);
		}

		void FunctionCall::ResolveTypesAndLifetimes(VariableContainer* varContainer, DiagnosticContainer* diagnosticContainer)
		{
			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				arguments[i]->ResolveTypesAndLifetimes(varContainer, diagnosticContainer);
			}

			TypeName funcReturnValue;
			if (varContainer->GetTypeName(target, funcReturnValue))
			{
				if (typeName != funcReturnValue)
				{
					typeName = funcReturnValue;
					varContainer->SetWriteFlag();
				}
			}
			else
			{
				if (varContainer->IsFinalPass())
				{
					// Only log diagnostic if previous passes didn't find this type
					diagnosticContainer->AddDiagnostic(span, "Expected return type from function");
				}
			}
		}

		Cast::~Cast()
		{
			delete target;
		}

		std::string Cast::ToString() const
		{
			StringBuilder stringBuilder;

			stringBuilder.Append("(");
			stringBuilder.Append(TypeNameToString(typeName));
			stringBuilder.Append(")");
			stringBuilder.Append(target->ToString());

			return stringBuilder.ToString();
		}

		Parser::Parser(Lexer* lexer, DiagnosticContainer* diagnosticContainer) :
			m_Lexer(lexer),
			diagnosticContainer(diagnosticContainer)
		{
			m_Current = m_Lexer->Next();
		}

		bool Parser::HasNext()
		{
			return !m_Lexer->sourceIter.EndOfFileReached();
		}

		bool Parser::NextIs(TokenKind tokenKind)
		{
			return m_Current.kind == tokenKind;
		}

		bool Parser::NextIsTypename()
		{
			return NextIs(TokenKind::INT_KEYWORD) ||
				NextIs(TokenKind::FLOAT_KEYWORD) ||
				NextIs(TokenKind::BOOL_KEYWORD) ||
				NextIs(TokenKind::STRING_KEYWORD) ||
				NextIs(TokenKind::CHAR_KEYWORD);
		}

		bool Parser::NextIsBinaryOperator()
		{
			// Handle "x-1" case where there's no whitespace between numbers
			if (NextIs(TokenKind::INT_LITERAL) || NextIs(TokenKind::FLOAT_LITERAL))
			{
				if (!m_Current.value.empty() && m_Current.value[0] == '-')
				{
					m_Current.kind = TokenKind::MINUS;
					m_Lexer->Backtrack();
					return true;
				}
			}

			return NextIs(TokenKind::PLUS) ||
				NextIs(TokenKind::MINUS) ||
				NextIs(TokenKind::STAR) ||
				NextIs(TokenKind::FOR_SLASH) ||
				NextIs(TokenKind::PERCENT) ||
				NextIs(TokenKind::EQUAL_EQUAL) ||
				NextIs(TokenKind::NOT_EQUAL) ||
				NextIs(TokenKind::GREATER) ||
				NextIs(TokenKind::GREATER_EQUAL) ||
				NextIs(TokenKind::LESS) ||
				NextIs(TokenKind::LESS_EQUAL) ||
				NextIs(TokenKind::BOOLEAN_AND) ||
				NextIs(TokenKind::BOOLEAN_OR) ||
				NextIs(TokenKind::BINARY_AND) ||
				NextIs(TokenKind::BINARY_OR) ||
				NextIs(TokenKind::BINARY_XOR);
		}

		bool Parser::NextIsCompoundAssignment()
		{
			return NextIs(TokenKind::PLUS_EQUALS) ||
				NextIs(TokenKind::MINUS_EQUALS) ||
				NextIs(TokenKind::STAR_EQUALS) ||
				NextIs(TokenKind::FOR_SLASH_EQUALS) ||
				NextIs(TokenKind::PERCENT_EQUALS) ||
				NextIs(TokenKind::BINARY_AND_EQUALS) ||
				NextIs(TokenKind::BINARY_OR_EQUALS) ||
				NextIs(TokenKind::BINARY_XOR_EQUALS);
		}

		Token Parser::Eat(TokenKind tokenKind)
		{
			if (NextIs(tokenKind))
			{
				Token previous = m_Current;
				m_Current = m_Lexer->Next();
				return previous;
			}

			StringBuilder diagnosticStr;
			diagnosticStr.Append("Expected \"");
			diagnosticStr.Append(TokenKindToString(tokenKind));
			diagnosticStr.Append("\" but found \"");
			diagnosticStr.Append(TokenKindToString(m_Current.kind));
			diagnosticStr.Append("\"");
			diagnosticContainer->AddDiagnostic(m_Current.span, diagnosticStr.ToString());

			return g_EmptyToken;
		}

		std::string Parser::NextTempIdentifier()
		{
			return "__tmp" + IntToString(m_TempIdentIdx++);
		}

		StatementBlock* Parser::NextStatementBlock()
		{
			Span span = Eat(TokenKind::OPEN_CURLY).span;
			std::vector<Statement*> statements;
			while (HasNext() && !NextIs(TokenKind::CLOSE_CURLY))
			{
				Statement* statement = NextStatement();
				if (statement == nullptr)
				{
					for (u32 i = 0; i < (u32)statements.size(); ++i)
					{
						delete statements[i];
					}

					return nullptr;
				}

				statements.push_back(statement);

				while (NextIs(TokenKind::SEMICOLON))
				{
					Eat(TokenKind::SEMICOLON);
				}
			}

			return new StatementBlock(span.Extend(Eat(TokenKind::CLOSE_CURLY).span), statements);
		}

		StatementBlock* Parser::Parse()
		{
			std::vector<Statement*> statements;
			Span span = Span(0, 0);

			while (!m_Lexer->sourceIter.EndOfFileReached())
			{
				Statement* statement = NextStatement();
				if (statement == nullptr)
				{
					for (u32 i = 0; i < (u32)statements.size(); ++i)
					{
						delete statements[i];
					}

					return nullptr;
				}

				statements.push_back(statement);

				while (NextIs(TokenKind::SEMICOLON))
				{
					Eat(TokenKind::SEMICOLON);
				}

				const i32 maxStatementCount = 100000;
				if (statements.size() > maxStatementCount)
				{
					PrintError("Maximum number of statements reached (%d), aborting\n", maxStatementCount);
					m_Lexer->diagnosticContainer->AddDiagnostic(statement->span, "Maximum number of statements reached, aborting");
					break;
				}
			}

			Eat(TokenKind::END_OF_FILE);

			return new StatementBlock(span.Extend(m_Lexer->GetSpan()), statements);
		}

		Expression* Parser::NextExpression()
		{
			Expression* expression = NextPrimary();

			if (NextIsBinaryOperator())
			{
				return NextBinary(0, expression);
			}
			else if (NextIs(TokenKind::QUESTION))
			{
				return NextTernary(expression);
			}

			return expression;
		}

		Expression* Parser::NextPrimary()
		{
			if (NextIsTypename())
			{
				Token typeToken = Eat(m_Current.kind);
				Span span = m_Current.span;

				TypeName typeName = TokenKindToTypeName(typeToken.kind);

				if (NextIs(TokenKind::OPEN_SQUARE))
				{
					Eat(TokenKind::OPEN_SQUARE);
					Eat(TokenKind::CLOSE_SQUARE);
					typeName = TypeNameToListVariant(typeName);
				}

				Token identifierToken = Eat(TokenKind::IDENTIFIER);

				Eat(TokenKind::EQUALS);
				Expression* initializer = NextExpression();
				if (initializer != nullptr)
				{
					//span = span.Extend(Eat(TokenKind::SEMICOLON).span);

					if (VM::VirtualMachine::IsTerminalOutputVar(identifierToken.value))
					{
						diagnosticContainer->AddDiagnostic(span, "Cannot overwrite system variable");
						delete initializer;
						return nullptr;
					}

					return new Declaration(span, identifierToken.value, initializer, typeName);
				}
			}
			else if (NextIs(TokenKind::OPEN_PAREN))
			{
				Eat(TokenKind::OPEN_PAREN);

				// Cast
				if (NextIsTypename())
				{
					Token typeToken = Eat(m_Current.kind);

					if (NextIs(TokenKind::CLOSE_PAREN))
					{
						Span span = m_Current.span;

						TypeName typeName = TokenKindToTypeName(typeToken.kind);

						span = span.Extend(Eat(TokenKind::CLOSE_PAREN).span);
						Expression* target = NextPrimary();
						if (target != nullptr)
						{
							// Ignore redundant casts
							if (target->typeName == typeName)
							{
								return target;
							}
							return new Cast(span, typeName, target);
						}
					}
				}
				else
				{
					Expression* subexpression = NextExpression();
					Eat(TokenKind::CLOSE_PAREN);
					return subexpression;
				}
			}
			else if (NextIs(TokenKind::INT_LITERAL))
			{
				Token intToken = Eat(TokenKind::INT_LITERAL);
				return new IntLiteral(intToken.span, ParseInt(intToken.value));
			}
			else if (NextIs(TokenKind::FLOAT_LITERAL))
			{
				Token floatToken = Eat(TokenKind::FLOAT_LITERAL);
				return new FloatLiteral(floatToken.span, ParseFloat(floatToken.value));
			}
			else if (NextIs(TokenKind::TRUE))
			{
				Token boolToken = Eat(TokenKind::TRUE);
				return new BoolLiteral(boolToken.span, true);
			}
			else if (NextIs(TokenKind::FALSE))
			{
				Token boolToken = Eat(TokenKind::FALSE);
				return new BoolLiteral(boolToken.span, false);
			}
			else if (NextIs(TokenKind::STRING_LITERAL))
			{
				Token stringToken = Eat(TokenKind::STRING_LITERAL);
				return new StringLiteral(stringToken.span, stringToken.value);
			}
			else if (NextIs(TokenKind::CHAR_LITERAL))
			{
				Token charToken = Eat(TokenKind::CHAR_LITERAL);
				return new CharLiteral(charToken.span, charToken.value[0]);
			}
			else if (NextIs(TokenKind::IDENTIFIER))
			{
				Token identifierToken = Eat(TokenKind::IDENTIFIER);
				Span span = identifierToken.span;

				if (NextIs(TokenKind::OPEN_PAREN))
				{
					Eat(TokenKind::OPEN_PAREN);
					std::vector<Expression*> argumentList = NextArgumentList();
					if (argumentList.size() != 1 || argumentList[0] != nullptr)
					{
						return new FunctionCall(span.Extend(Eat(TokenKind::CLOSE_PAREN).span), identifierToken.value, argumentList);
					}
				}
				else if (NextIs(TokenKind::OPEN_SQUARE))
				{
					Eat(TokenKind::OPEN_SQUARE);
					Expression* indexExpression = NextExpression();
					Eat(TokenKind::CLOSE_SQUARE);

					if (indexExpression != nullptr)
					{
						return new IndexOperation(span.Extend(indexExpression->span), identifierToken.value, indexExpression);
					}
				}
				else if (NextIs(TokenKind::EQUALS))
				{
					Eat(TokenKind::EQUALS);
					Expression* rhs = NextExpression();
					if (rhs != nullptr)
					{
						rhs->span = rhs->span.Extend(Eat(TokenKind::SEMICOLON).span);
						return new Assignment(span.Extend(rhs->span), identifierToken.value, rhs);
					}

				}
				else if (NextIsCompoundAssignment())
				{
					Token opToken = Eat(m_Current.kind);

					Expression* rhs = NextExpression();
					if (rhs != nullptr)
					{
						return new CompoundAssignment(span.Extend(rhs->span), identifierToken.value, rhs, TokenKindToBinaryOperatorType(opToken.kind));
					}
				}
				else if (NextIs(TokenKind::PLUS_PLUS) || NextIs(TokenKind::MINUS_MINUS))
				{
					diagnosticContainer->AddDiagnostic(m_Current.span, "Increment/decrement operator not supported!");
					Eat(m_Current.kind);
				}

				return new Identifier(span, identifierToken.value);
			}
			else if (NextIs(TokenKind::TILDE))
			{
				return NextUnary();
			}
			else if (NextIs(TokenKind::BANG))
			{
				return NextUnary();
			}
			else if (NextIs(TokenKind::OPEN_CURLY))
			{
				return NextListInitializer();
			}
			else if (NextIs(TokenKind::PLUS_PLUS) || NextIs(TokenKind::MINUS_MINUS))
			{
				diagnosticContainer->AddDiagnostic(m_Current.span, "Increment/decrement operator not supported!");
				Eat(m_Current.kind);
				return nullptr;
			}
			else if (NextIs(TokenKind::MINUS))
			{
				return NextUnary();
			}

			StringBuilder diagnosticStr;
			diagnosticStr.Append("Expected expression, but found \"");
			diagnosticStr.Append(TokenKindToString(m_Current.kind));
			diagnosticStr.Append("\"");
			diagnosticContainer->AddDiagnostic(m_Current.span, diagnosticStr.ToString());

			return nullptr;
		}

		Expression* Parser::NextBinary(i32 lhsPrecendence, Expression* lhs)
		{
			while (true)
			{
				if (lhs == nullptr)
				{
					return nullptr;
				}

				i32 precedence = GetBinaryOperatorPrecedence(m_Current.kind);
				if (precedence < lhsPrecendence)
				{
					return lhs;
				}

				Expression* rhs = nullptr;
				if (m_Current.kind == TokenKind::QUESTION)
				{
					rhs = NextTernary(lhs);
					if (rhs == nullptr)
					{
						delete lhs;
						return nullptr;
					}
					return rhs;
				}

				Token binaryOperator = Eat(m_Current.kind);

				{
					rhs = NextPrimary();
					if (rhs == nullptr)
					{
						delete lhs;
						return nullptr;
					}
				}

				i32 nextPrecedence = GetBinaryOperatorPrecedence(m_Current.kind);
				if (precedence < nextPrecedence)
				{
					Expression* nextRHS = NextBinary(precedence + 1, rhs);
					if (nextRHS != nullptr)
					{
						rhs = nextRHS;
					}
					else
					{
						delete lhs;
						return nullptr;
					}
				}

				lhs = new BinaryOperation(lhs->span.Extend(rhs->span), TokenKindToBinaryOperatorType(binaryOperator.kind), lhs, rhs);
			}
		}

		UnaryOperation* Parser::NextUnary()
		{
			Token unaryOperatorToken = Eat(m_Current.kind);
			Expression* expression = NextExpression();
			if (expression != nullptr)
			{
				return new UnaryOperation(unaryOperatorToken.span.Extend(expression->span), TokenKindToUnaryOperatorType(unaryOperatorToken.kind), expression);
			}

			return nullptr;
		}

		TernaryOperation* Parser::NextTernary(Expression* condition)
		{
			if (condition == nullptr)
			{
				return nullptr;
			}

			Eat(TokenKind::QUESTION);
			Expression* ifTrue = NextExpression();
			if (ifTrue != nullptr)
			{
				Eat(TokenKind::SINGLE_COLON);
				Expression* ifFalse = NextExpression();
				if (ifFalse != nullptr)
				{
					return new TernaryOperation(condition->span.Extend(ifFalse->span), condition, ifTrue, ifFalse);
				}
			}

			delete ifTrue;
			return nullptr;
		}

		ListInitializer* Parser::NextListInitializer()
		{
			Span span = Eat(TokenKind::OPEN_CURLY).span;

			std::vector<Expression*> listValues;
			if (!NextIs(TokenKind::CLOSE_CURLY))
			{
				while (true)
				{
					Expression* expression = NextExpression();
					if (expression == nullptr)
					{
						for (u32 i = 0; i < (u32)listValues.size(); ++i)
						{
							delete listValues[i];
						}
						return nullptr;
					}

					listValues.push_back(expression);

					if (NextIs(TokenKind::CLOSE_CURLY))
					{
						break;
					}

					Eat(TokenKind::COMMA);
				}
			}

			return new ListInitializer(span.Extend(Eat(TokenKind::CLOSE_CURLY).span), listValues);
		}

		Statement* Parser::NextStatement()
		{
			if (NextIs(TokenKind::IDENTIFIER))
			{
				return NextExpression();
			}
			else if (NextIsTypename())
			{
				return NextExpression();
			}
			else if (NextIs(TokenKind::OPEN_CURLY))
			{
				return NextStatementBlock();
			}
			else if (NextIs(TokenKind::IF))
			{
				return NextIfStatement();
			}
			else if (NextIs(TokenKind::FOR))
			{
				return NextForStatement();
			}
			else if (NextIs(TokenKind::DO))
			{
				return NextDoWhileStatement();
			}
			else if (NextIs(TokenKind::WHILE))
			{
				return NextWhileStatement();
			}
			else if (NextIs(TokenKind::FUNC))
			{
				return NextFunctionDeclaration();
			}
			else if (NextIs(TokenKind::BREAK))
			{
				Token breakToken = Eat(TokenKind::BREAK);
				Eat(TokenKind::SEMICOLON);
				return new BreakStatement(breakToken.span);
			}
			else if (NextIs(TokenKind::YIELD))
			{
				Token yieldToken = Eat(TokenKind::YIELD);

				Expression* yieldValue = nullptr;
				if (!NextIs(TokenKind::SEMICOLON))
				{
					yieldValue = NextExpression();
				}
				Eat(TokenKind::SEMICOLON);

				return new YieldStatement(yieldToken.span.Extend(Eat(TokenKind::SEMICOLON).span), yieldValue);
			}
			else if (NextIs(TokenKind::RETURN))
			{
				Token returnToken = Eat(TokenKind::RETURN);

				Expression* returnValue = nullptr;
				if (!NextIs(TokenKind::SEMICOLON))
				{
					returnValue = NextExpression();
				}
				Eat(TokenKind::SEMICOLON);

				return new ReturnStatement(returnToken.span, returnValue);
			}

			StringBuilder diagnosticStr;
			diagnosticStr.Append("Expected expression, but found \"");
			diagnosticStr.Append(TokenKindToString(m_Current.kind));
			diagnosticStr.Append("\"");
			diagnosticContainer->AddDiagnostic(m_Current.span, diagnosticStr.ToString());

			Eat(m_Current.kind);

			return nullptr;
		}

		IfStatement* Parser::NextIfStatement()
		{
			if (!NextIs(TokenKind::IF) && !NextIs(TokenKind::ELIF))
			{
				std::string tokenKindStr(TokenKindToString(m_Current.kind));
				diagnosticContainer->AddDiagnostic(m_Current.span, "Expected if or elif, instead got " + tokenKindStr);
				return nullptr;
			}

			Span span = Eat(m_Current.kind).span;
			Eat(TokenKind::OPEN_PAREN);
			Expression* condition = NextExpression();
			if (condition != nullptr)
			{
				Eat(TokenKind::CLOSE_PAREN);
				Statement* then = NextStatement();
				if (then != nullptr)
				{
					span = span.Extend(then->span);

					Statement* otherwise = nullptr;
					if (NextIs(TokenKind::ELIF))
					{
						otherwise = NextIfStatement();
						if (otherwise == nullptr)
						{
							delete condition;
							delete then;
							return nullptr;
						}
					}
					else if (NextIs(TokenKind::ELSE))
					{
						Eat(TokenKind::ELSE);
						otherwise = NextStatement();
						if (otherwise == nullptr)
						{
							delete condition;
							delete then;
							return nullptr;
						}
						span = span.Extend(otherwise->span);
					}

					return new IfStatement(span, condition, then, otherwise);
				}
			}

			return nullptr;
		}

		ForStatement* Parser::NextForStatement()
		{
			Span span = Eat(TokenKind::FOR).span;
			Expression* setup = nullptr;
			Expression* condition = nullptr;
			Statement* update = nullptr;
			Eat(TokenKind::OPEN_PAREN);
			if (!NextIs(TokenKind::SEMICOLON))
			{
				setup = NextExpression();
				if (setup == nullptr)
				{
					return nullptr;
				}
			}
			Eat(TokenKind::SEMICOLON);
			if (!NextIs(TokenKind::SEMICOLON))
			{
				condition = NextExpression();
				if (condition == nullptr)
				{
					delete setup;
					return nullptr;
				}
			}
			Eat(TokenKind::SEMICOLON);
			if (!NextIs(TokenKind::CLOSE_PAREN))
			{
				update = NextExpression();
				if (update == nullptr)
				{
					delete setup;
					delete condition;
					return nullptr;
				}
			}
			Eat(TokenKind::CLOSE_PAREN);
			Statement* body = NextStatement();
			if (body != nullptr)
			{
				return new ForStatement(span.Extend(body->span), setup, condition, update, body);
			}

			delete setup;
			delete condition;
			delete update;
			return nullptr;
		}

		WhileStatement* Parser::NextWhileStatement()
		{
			Span span = Eat(TokenKind::WHILE).span;
			Eat(TokenKind::OPEN_PAREN);
			Expression* condition = NextExpression();
			if (condition != nullptr)
			{
				Eat(TokenKind::CLOSE_PAREN);
				Statement* body = NextStatement();
				if (body != nullptr)
				{
					return new WhileStatement(span.Extend(body->span), condition, body);
				}
			}

			delete condition;
			return nullptr;
		}

		DoWhileStatement* Parser::NextDoWhileStatement()
		{
			Span span = Eat(TokenKind::DO).span;
			Statement* body = NextStatement();
			if (body != nullptr)
			{
				Eat(TokenKind::WHILE);
				Eat(TokenKind::OPEN_PAREN);
				Expression* condition = NextExpression();
				if (condition != nullptr)
				{
					Eat(TokenKind::CLOSE_PAREN);
					return new DoWhileStatement(span.Extend(Eat(TokenKind::SEMICOLON).span), condition, body);
				}
			}

			delete body;
			return nullptr;
		}

		FunctionDeclaration* Parser::NextFunctionDeclaration()
		{
			Span span = Eat(TokenKind::FUNC).span;
			Token functionNameToken = Eat(TokenKind::IDENTIFIER);
			Eat(TokenKind::OPEN_PAREN);
			std::vector<Declaration*> arguments = NextArgumentDefinitionList();
			Eat(TokenKind::CLOSE_PAREN);
			Eat(TokenKind::ARROW);
			Token returnTypeToken = Eat(m_Current.kind);
			StatementBlock* body = NextStatementBlock();
			if (body != nullptr)
			{
				return new FunctionDeclaration(span.Extend(body->span), functionNameToken.value, arguments, TokenKindToTypeName(returnTypeToken.kind), body);
			}

			for (u32 i = 0; i < (u32)arguments.size(); ++i)
			{
				delete arguments[i];
			}
			return nullptr;
		}

		std::vector<Declaration*> Parser::NextArgumentDefinitionList()
		{
			std::vector<Declaration*> result;

			if (!NextIs(TokenKind::CLOSE_PAREN))
			{
				while (true)
				{
					Token typeToken = Eat(m_Current.kind);
					Span span = typeToken.span;
					Token identifier = Eat(TokenKind::IDENTIFIER);

					if (VM::VirtualMachine::IsTerminalOutputVar(identifier.value))
					{
						diagnosticContainer->AddDiagnostic(span, "Cannot overwrite system variable");
						// TODO: Memory leak - delete list of previous results
						return {};
					}

					result.push_back(new Declaration(span.Extend(identifier.span), identifier.value, nullptr, TokenKindToTypeName(typeToken.kind), true));

					if (!NextIs(TokenKind::COMMA))
					{
						break;
					}

					Eat(TokenKind::COMMA);
				}
			}

			return result;
		}

		std::vector<Expression*> Parser::NextArgumentList()
		{
			std::vector<Expression*> result;

			if (!NextIs(TokenKind::CLOSE_PAREN))
			{
				while (true)
				{
					Expression* argument = NextExpression();
					if (argument == nullptr)
					{
						return { nullptr };
					}

					result.push_back(argument);

					if (!NextIs(TokenKind::COMMA))
					{
						break;
					}

					Eat(TokenKind::COMMA);
				}
			}

			return result;
		}

		AST::AST()
		{
		}

		AST::~AST()
		{
			delete rootBlock;
		}

		void AST::Generate(const char* sourceText)
		{
			bValid = false;

			diagnosticContainer = new DiagnosticContainer();
			lexer = new Lexer(sourceText, diagnosticContainer);
			parser = new Parser(lexer, diagnosticContainer);

			rootBlock = parser->Parse();

			bValid = true;
		}

		void AST::Destroy()
		{
			bValid = false;

			delete diagnosticContainer;
			diagnosticContainer = nullptr;
			delete rootBlock;
			rootBlock = nullptr;
			delete parser;
			parser = nullptr;
		}
	} // namespace AST
} // namespace flex
