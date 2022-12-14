#include "parse.h"

#include <stdlib.h>
#include <stdio.h>

#include "scanner.h"
#include "node.h"
#include "token.h"


static void error(const char *message, Token last)
{
	fprintf(stderr, "parsing error: %s ", message);
	if (last.type == END_TOKEN) {
		fprintf(stderr, "(while parsing 'EOF')\n");
	} else {
		fprintf(stderr, "(while parsing '%.*s')\n", last.length, last.string);
	}
}

// subexpression parsers (from lowest to highest priority)
static Node *parse_expression(Scanner *scanner);
static Node *parse_if(Scanner *s);
static Node *parse_or(Scanner *s);
static Node *parse_and(Scanner *s);
static Node *parse_cmp(Scanner *s);
static Node *parse_sum(Scanner *s);
static Node *parse_product(Scanner *s);
static Node *parse_expt(Scanner *s);
static Node *parse_term(Scanner *s);

// VALID ::= EXPRESSION 'END'
Node *parse(Scanner *scanner)
{
	Node *expr = parse_expression(scanner);
	if (!expr) {
		return NULL;
	}
	Token next = Scanner_peek(scanner);
	if (next.type != END_TOKEN) {
		error("unexpected token after the expression", next);
		Node_drop(expr);
		return NULL;
	}
	return expr;
}

// EXPRESSION ::= IF | OR
static Node *parse_expression(Scanner *scanner)
{
	Token next = Scanner_peek(scanner);
	if (next.type == IF_TOKEN) {
		return parse_if(scanner);
	}
	return parse_or(scanner);
}

// IF ::= 'IF' OR 'THEN' OR 'ELSE' OR
static Node *parse_if(Scanner *scanner)
{
	Scanner_next(scanner);
	Node *cond = parse_or(scanner);
	if (!cond) {
		return NULL;
	}
	Token next = Scanner_next(scanner);
	if (next.type != THEN_TOKEN) {
		error("expected 'then'", next);
		return NULL;
	}
	Node *true = parse_or(scanner);
	if (!true) {
		return NULL;
	}
	next = Scanner_next(scanner);
	if (next.type != ELSE_TOKEN) {
		error("expected 'else'", next);
		return NULL;
	}
	Node *false = parse_or(scanner);
	if (!false) {
		return NULL;
	}
	return IfNode_new(cond, true, false);
}

// OR ::= AND {'OR' AND}
static Node *parse_or(Scanner *scanner)
{
	Node *left = parse_and(scanner);
	if (!left) {
		return NULL;
	}
	for (;;) {
		Token next = Scanner_peek(scanner);
		if (next.type != OR_TOKEN) {
			return left;
		}
		Scanner_next(scanner);
		Node *right = parse_and(scanner);
		if (!right) {
			Node_drop(left);
			return NULL;
		}
		left = OrNode_new(left, right);
	}
}

// AND ::= CMP {'AND' CMP}
static Node *parse_and(Scanner *scanner)
{
	Node *left = parse_cmp(scanner);
	if (!left) {
		return NULL;
	}
	for (;;) {
		Token next = Scanner_peek(scanner);
		if (next.type != AND_TOKEN) {
			return left;
		}
		Scanner_next(scanner);
		Node *right = parse_cmp(scanner);
		if (!right) {
			Node_drop(left);
			return NULL;
		}
		left = AndNode_new(left, right);
	}
}

// CMP ::= SUM ['>' SUM]
static Node *parse_cmp(Scanner *scanner)
{
	Node *left = parse_sum(scanner);
	if (!left) {
		return NULL;
	}
	Token next = Scanner_peek(scanner);
	if (next.type == GT_TOKEN) {
		Scanner_next(scanner);
		Node *right = parse_sum(scanner);
		if (!right) {
			return NULL;
		}
		return CmpNode_new(left, right);
	}
	return left;
}

// TODO: implement subtraction
// SUM ::= PRODUCT {'+' PRODUCT}
static Node *parse_sum(Scanner *scanner)
{
	Node *left = parse_product(scanner);
	if (!left) {
		return NULL;
	}
	for (;;) {
		Token next = Scanner_peek(scanner);
		if (next.type != PLUS_TOKEN) {
			return left;
		}
		Scanner_next(scanner);
		Node *right = parse_product(scanner);
		if (!right) {
			Node_drop(left);
			return NULL;
		}
		left = SumNode_new(left, right);
	}
}

// TODO: implement division
// PRODUCT ::= EXPT {'*' EXPT}
static Node *parse_product(Scanner *scanner)
{
	Node *left = parse_expt(scanner);
	if (!left) {
		return NULL;
	}
	for (;;) {
		Token next = Scanner_peek(scanner);
		if (next.type != ASTERISK_TOKEN) {
			return left;
		}
		Scanner_next(scanner);
		Node *right = parse_expt(scanner);
		if (!right) {
			Node_drop(left);
			return NULL;
		}
		left = ProductNode_new(left, right);
	}
}

// EXPT ::= TERM | TERM '^' EXPT
static Node *parse_expt(Scanner *scanner)
{
	Node *base = parse_term(scanner);
	if (!base) {
		return NULL;
	}
	Token next = Scanner_peek(scanner);
	if (next.type == CARET_TOKEN) {
		Scanner_next(scanner);
		Node *exponent = parse_expt(scanner);
		if (!exponent) {
			Node_drop(base);
			return NULL;
		}
		return ExptNode_new(base, exponent);
	}
	return base;
}

// TERM ::= '(' EXPRESSION ')' | 'NUMBER'
static Node *parse_term(Scanner *scanner)
{
	Token next = Scanner_next(scanner);
	if (next.type == LPAREN_TOKEN) {
		Node *expr = parse_expression(scanner);
		if (!expr) {
			return NULL;
		}
		next = Scanner_next(scanner);
		if (next.type != RPAREN_TOKEN) {
			error("expected ')'", next);
			Node_drop(expr);
			return NULL;
		}
		return expr;
	} else if (next.type == NUMBER_TOKEN) {
		return NumberNode_new(next.string, next.length);
	} else {
		error("expected '(' or a number", next);
		return NULL;
	}
}
