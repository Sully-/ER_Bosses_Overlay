#include <string>
#include <iostream>
#include "scaling.hpp"
#include "../config.hpp"
#include "../util/exprtk.hpp"

namespace er::scaling
{
	typedef exprtk::symbol_table<double> symbol_table_t;
	typedef exprtk::expression<double> expression_t;
	typedef exprtk::parser<double> parser_t;

	int Score::computeScore(uint8_t scaling)
	{
		// nlogn lookup, we might need to cache this.
		std::string formula = er::gConfig.get("boss.score_formula", "1");

		symbol_table_t symbol_table;
		double scaling_value = scaling;

		symbol_table.add_variable("scaling_value", scaling_value);

		expression_t expression;
		expression.register_symbol_table(symbol_table);

		parser_t parser;

		if (parser.compile(formula, expression)) {
			return expression.value();
		}
		else {
			std::cerr << "Error: " << parser.error() << std::endl;
		}

		return 1;
	}
}
