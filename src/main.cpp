/*
Program 13 - Huffman
Napisać program do kompresji plików metodą Huffmanna.
Program uruchamiany jest z linii poleceń z wykorzystaniem następujących przełączników:
-i plik wejściowy
-o plik wyjściowy
-t tryb: k – kompresja, d – dekompresja
-s plik ze słownikiem (tworzonym w czasie kompresji, używanym w czasie
dekompresji)
*/
#include <boost/json/parse_options.hpp>
#include <boost/json/storage_ptr.hpp>
#include <boost/json/stream_parser.hpp>
#include <boost/json/value.hpp>
#include <boost/property_tree/ptree_fwd.hpp>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <getopt.h>
#include <huffman.hpp>
#include <boost/json/src.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#ifndef VERSION
#define VERSION "unknown"
#endif

namespace pt = boost::property_tree;

static struct
{
	const char shortopt;
	const char* longopt;
	int arg_type;
	const char* argname;
	const char* description;
}
options[] =
{
	{'h', "help",			no_argument,		NULL,		"display this message and exit"},
	{'i', "input",			required_argument,	"FILE",		"input file (only one)"},
	{'o', "output",			required_argument,	"FILE",		"output file (only one)"},
	{'m', "mode",			required_argument,	"MODE",		"'c' or 'k' for compression, 'd' for decompression"},
	{'t', NULL,				required_argument,	"",			"same as --mode (added to meet project's specifications)"},
	{'d', "dictionary",		required_argument,	"FILE",		"file to read/write the dictionary"},
	{'s', NULL,				required_argument,	"FILE",		"same as --dictionary (added to meet project's specifications)"},
	{'v', "version",		no_argument,		NULL,		"show the program's version"},
};

static struct
{
	enum{not_specified, compression, decompression} mode = not_specified;
	std::string dictionary_path{};
	std::string input_path{};
	std::string output_path{};

	bool is_initialized() const
	{
		return mode != not_specified && !dictionary_path.empty() && !input_path.empty() && !output_path.empty();
	}
}
parsed_options{};

static void print_help(const char* prog_name)
{
	auto print_element = [](const char shortopt, const char* longopt, int arg_type, const char* argname, const char* description)
	{
		(void) arg_type;
		std::cout << std::setfill(' ')
			<< std::right << std::setw(4) << (shortopt ? std::string("-") + shortopt : "")
			<< std::left << std::setw(25) << (longopt ? (shortopt ? "," : " ") + std::string(" --") + longopt + (argname ? std::string("=") + argname : "") : "")
			<< description
			<< std::endl;
	};

	std::cout
		<< "Usage: " << prog_name << " options..." << std::endl
		<< "Compress files using Huffman compression." << std::endl
		<< std::endl
		<< "Mandatory arguments to long options are mandatory for short options too." << std::endl;

	for(size_t i = 0; i < sizeof(options)/sizeof(*options); i++)
	{
		auto& opt = options[i];
		print_element(opt.shortopt, opt.longopt, opt.arg_type, opt.argname, opt.description);
	}
}

static int parse_args(int argc, char* argv[])
{
	int option;
	int option_index = 0;
	int should_parse = 1;

	std::vector<struct option> long_options;

	for(size_t i = 0; i < sizeof(options)/sizeof(*options); i++)
	{
		auto& fullopt = options[i];

		long_options.push_back({fullopt.longopt ? fullopt.longopt : "", fullopt.arg_type, 0, fullopt.shortopt});
	}
	long_options.push_back({});

	while(should_parse)
	{
		option = getopt_long(argc, argv, "vhi:o:m:t:d:s:", long_options.data(), &option_index);
		switch(option)
		{
			case 0:
			break;

			case 'h':
				print_help(*argv);
				exit(EXIT_SUCCESS);
			break;

			case 'v':
				std::cout << *argv << ": version " VERSION << std::endl;
				exit(EXIT_SUCCESS);
			break;

			case 'i':
				parsed_options.input_path = optarg;
			break;

			case 'o':
				parsed_options.output_path = optarg;
			break;

			case 't':
			case 'm':
				if(*optarg == 'c' || *optarg == 'k')
				{
					parsed_options.mode = parsed_options.compression;
				}
				else if(*optarg == 'd')
				{
					parsed_options.mode = parsed_options.decompression;
				}
				else
				{
					std::cout << "Invalid mode" << std::endl;
					print_help(argv[0]);
					exit(EXIT_FAILURE);
				}
			break;

			case 's':
			case 'd':
				parsed_options.dictionary_path = optarg;
			break;

			case '?':
				std::cerr << "Try '" << *argv << " --help' for more information." << std::endl;
				exit(EXIT_FAILURE);
			break;

			default:
				should_parse = 0;
			break;
		}
	}

	if(parsed_options.is_initialized() == false)
	{
		print_help(argv[0]);
		exit(EXIT_FAILURE);
	}

	return 0;
}

std::unique_ptr<huffman_tree_node> read_huffman_tree_json(const char* path)
{
	pt::ptree root;
	pt::read_json(path, root);
	std::unique_ptr<huffman_tree_node> result{};

	std::function<std::unique_ptr<huffman_tree_node>(const pt::ptree&)> recursive_get =
	[&](const pt::ptree& json_node)
	{
		auto new_node = std::make_unique<huffman_tree_node>();

		try
		{
			new_node->m_frequency = json_node.get<size_t>("frequency");
		}
		catch(...)
		{
			return std::unique_ptr<huffman_tree_node>{};
		}

		try
		{
			new_node->m_left = recursive_get(json_node.get_child("left"));
			new_node->m_right = recursive_get(json_node.get_child("right"));

			if(!(new_node->m_left && new_node->m_right))
			{
				throw std::runtime_error("");
			}
		}
		catch(...)
		{
			try
			{
				new_node->m_character = json_node.get<char>("character");
			}
			catch(...)
			{
				return std::unique_ptr<huffman_tree_node>{};
			}
		}

		return new_node;
	};

	try
	{
		auto real_root = root.get_child("root");
		result = recursive_get(real_root);
	}
	catch(...)
	{

	}

	return result;
}

bool write_huffman_tree_json(const char* path, const huffman_tree_node& root)
{
	pt::ptree json_root, json_root_node;
	std::ofstream file{path};

	std::function<void(pt::ptree&, const huffman_tree_node&)> recursive_put =
	[&](pt::ptree& json_node, const huffman_tree_node& node)
	{
		if(node.is_character())
		{
			json_node.put("character", node.m_character);
			json_node.put("frequency", node.m_frequency);
		}
		else if(node.m_left && node.m_right)
		{
			pt::ptree left, right;

			recursive_put(left, *node.m_left);
			recursive_put(right, *node.m_right);

			json_node.add_child("left", left);
			json_node.add_child("right", right);
		}
	};

	recursive_put(json_root_node, root);

	json_root.add_child("root", json_root_node);

	//pt::write_json(file, json_root);
	pt::write_json(std::cout, json_root);
	(void)path;

	return true;
}

int main(int argc, char* argv[])
{
	parse_args(argc, argv);

	if(parsed_options.mode == parsed_options.compression)
	{
		char read_buffer[1024], write_buffer[1024];
		std::ifstream input_file(parsed_options.input_path);
		std::ofstream output_file(parsed_options.output_path);
		HuffmanDictionary dictionary;

		if(!input_file || !output_file)
		{
			// TODO
			exit(EXIT_FAILURE);
		}

		size_t read_bytes;
		while((read_bytes = input_file.readsome(read_buffer, sizeof(read_buffer))))
		{
			dictionary.create_part(read_buffer, read_bytes);
		}

		input_file.seekg(0);

		size_t offset = 0;
		while((read_bytes = input_file.readsome(read_buffer, sizeof(read_buffer))))
		{
			memset(write_buffer, 0, sizeof(write_buffer));
			offset = dictionary.encode(read_buffer, read_bytes, write_buffer, sizeof(write_buffer), offset).second;
			size_t buffer_fill = offset/8;
			offset -= (offset/8)*8;

			if(!output_file.write(write_buffer, buffer_fill))
			{
				// TODO
				exit(EXIT_FAILURE);
			}

			memmove(write_buffer, write_buffer+buffer_fill, 1);
		}

		if(offset)
		{
			if(!output_file.write(write_buffer, 1))
			{
				// TODO
				exit(EXIT_FAILURE);
			}
		}

		if(!write_huffman_tree_json(parsed_options.dictionary_path.c_str(), *dictionary.data()))
		{
			// TODO
			exit(EXIT_FAILURE);
		}

		output_file.close();
		input_file.close();
	}
	else
	{

	}
}
