#include <limits>
#include <map>
#include <functional>
#include <memory>
#include <vector>
#include <iostream>

#include <huffman.hpp>

using sorted_frequencies = std::vector<std::unique_ptr<huffman_tree_node>>;

huffman_tree_node::huffman_tree_node(char c, size_t freq)
	:m_character{c},
	m_frequency{freq}
{

}

huffman_tree_node::huffman_tree_node(std::unique_ptr<huffman_tree_node>&& lhs, std::unique_ptr<huffman_tree_node>&& rhs, size_t freq)
	:m_left{std::move(lhs)},
	m_right{std::move(rhs)},
	m_frequency{freq}
{

}

bool huffman_tree_node::is_character() const
{
	return !(m_left.get() || m_right.get());
}

huffman_tree_node::huffman_tree_node(const huffman_tree_node& node)
{
	*this = node;
}

huffman_tree_node::huffman_tree_node(huffman_tree_node&& node)
{
	*this = std::move(node);
}

huffman_tree_node& huffman_tree_node::operator=(const huffman_tree_node& node)
{
	m_character = node.m_character;
	m_frequency = node.m_frequency;

	if(node.m_left && node.m_right)
	{
		m_left = std::make_unique<huffman_tree_node>(*node.m_left);
		m_right = std::make_unique<huffman_tree_node>(*node.m_right);
	}

	return *this;
}

huffman_tree_node& huffman_tree_node::operator=(huffman_tree_node&& node)
{
	m_character = node.m_character;
	m_frequency = node.m_frequency;

	m_left = std::move(node.m_left);
	m_right = std::move(node.m_right);

	return *this;
}

sorted_frequencies get_frequencies(const char* data, size_t size)
{
	const void* end = data + size;
	size_t freqs[256]{};
	sorted_frequencies result;

	// Get frequencies of each character
	while(data < end)
	{
		freqs[(unsigned char)*data]++;
		data++;
	}

	// Create nodes for characters with frequency > 0
	for(size_t i = 0; i < 256; i++)
	{
		if(freqs[i] == 0) continue;

		// Insert the node into result
		result.emplace(std::find_if(result.begin(),
								result.end(),
								[&](const std::unique_ptr<huffman_tree_node>& n)
								{ return n->m_frequency >= freqs[i]; }),
								std::make_unique<huffman_tree_node>(i, freqs[i]));
	}

	return result;
}

HuffmanDictionary::HuffmanDictionary(const huffman_tree_node& root)
	: m_root{std::make_unique<huffman_tree_node>(root)}
{
	
}

HuffmanDictionary::HuffmanDictionary(const HuffmanDictionary& node)
{
	*this = node;
}

HuffmanDictionary::HuffmanDictionary(HuffmanDictionary&& node)
{
	*this = std::move(node);
}

HuffmanDictionary& HuffmanDictionary::operator=(const HuffmanDictionary& node)
{
	if(node.m_root)
	{
		m_root = std::make_unique<huffman_tree_node>(*node.m_root);
	}
	else
	{
		m_root = nullptr;
	}
	
	return *this;
}

HuffmanDictionary& HuffmanDictionary::operator=(HuffmanDictionary&& node)
{
	m_root = std::move(node.m_root);
	
	return *this;
}

void HuffmanDictionary::create(const char* data, size_t size)
{
	m_root = {};
	create_part(data, size);
}

const huffman_tree_node* HuffmanDictionary::data() const
{
	return m_root.get();
}

void HuffmanDictionary::create_part(const char* data, size_t size)
{
	sorted_frequencies frequencies = get_frequencies(data, size);
	std::vector<std::pair<char, size_t>> old_frequencies;

	std::function<void(const huffman_tree_node& node)> get_old_frequencies =
	[&](const huffman_tree_node& node)
	{
		if(node.is_character())
		{
			old_frequencies.emplace_back(node.m_character, node.m_frequency);
		}
		else
		{
			get_old_frequencies(*node.m_left);
			get_old_frequencies(*node.m_right);
		}
	};

	if(is_initialized())
	{
		get_old_frequencies(*m_root);

		for(const auto& old_freq : old_frequencies)
		{
			bool set = false;
			for(auto& new_freq : frequencies)
			{
				if(new_freq->m_character == old_freq.first)
				{
					new_freq->m_frequency += old_freq.second;
					set = true;
					break;
				}
			}

			if(set == false)
			{
				frequencies.emplace(std::find_if(frequencies.begin(),
								frequencies.end(),
								[&](const std::unique_ptr<huffman_tree_node>& n)
								{ return n->m_frequency >= old_freq.second; }),
								std::make_unique<huffman_tree_node>(old_freq.first, old_freq.second));
			}
		}
	}

	while(frequencies.size() > 1)
	{
		size_t new_frequency = frequencies[0]->m_frequency + frequencies[1]->m_frequency;
		auto new_node = std::make_unique<huffman_tree_node>(std::move(frequencies[0]), std::move(frequencies[1]), new_frequency);

		// Remove the first two nodes
		frequencies.erase(frequencies.begin(), frequencies.begin()+2);

		// Insert the new node
		frequencies.insert(std::find_if(frequencies.begin(),
									frequencies.end(),
									[&](const std::unique_ptr<huffman_tree_node>& n)
									{ return n->m_frequency >= new_frequency; }),
									std::move(new_node));
	}

	if(frequencies.size() != 0)
		m_root = std::move(frequencies.front());
	else
		m_root = {};
}

size_t HuffmanDictionary::size() const
{
	if(!is_initialized()) return 0;

	return m_root->m_frequency;
}

bool HuffmanDictionary::is_initialized() const
{
	return m_root.get();
}

std::pair<size_t, size_t> HuffmanDictionary::encode(const char* src, size_t src_size, char* dst, size_t dst_size, size_t offset)
{
	std::map<char, std::pair<char, uint8_t>> lookup_table{};

	// When travelling down the tree the code reversed, so we need to reverse it once again
	auto reverse_code = [](char code, uint8_t depth)
	{
		char new_code = 0;
		while(depth--)
		{
			new_code |= (code & 1) << depth;
			code >>= 1;
		}

		return new_code;
	};
	
	// Fills up an std::map that contains information to translate characters into their corresponding code
	std::function<void(const huffman_tree_node& node, char code, uint8_t depth)> make_lookup_table =
	[&](const huffman_tree_node& node, char code, uint8_t depth)
	{
		if(node.is_character())
		{
			lookup_table.emplace(node.m_character, std::make_pair(reverse_code(code, depth), depth));
		}
		else
		{
			make_lookup_table(*node.m_right, code << 1, depth+1);
			make_lookup_table(*node.m_left, (code << 1) | 1, depth+1);
		}
	};

	if(this->is_initialized() == false || src_size == 0 || dst_size == 0)
	{
		return {0, 0};
	}

	if(src_size > std::numeric_limits<size_t>::max()/8)
	{
		src_size = std::numeric_limits<size_t>::max()/8;
	}

	make_lookup_table(*this->m_root, 0, 0);

	size_t used_bits = offset % 8;
	size_t dst_index = offset / 8;
	char byte = dst[dst_index];

	for(size_t i = 0; i < src_size; i++)
	{
		std::pair<char, uint8_t> code;
		char c = src[i];
		int space_left;

		try
		{
			code = lookup_table.at(c);
		}
		catch(...)
		{
			return {0, 0};
		}

		space_left = 8 - used_bits - code.second;

		byte |= code.first << used_bits;

		if(space_left <= 0)
		{
			if(dst_size--)
			{
				dst[dst_index++] = byte;
				byte = (dst_size ? dst[dst_index] >> used_bits : 0) | (code.first >> (8-used_bits));
				used_bits = -space_left;
			}
			else
			{
				return {i, dst_index*8};
			}
		}
		else
		{
			used_bits = 8 - space_left;
		}
	}

	if(used_bits && dst_size)
	{
		dst[dst_index] = byte;
		return {src_size, dst_index*8 + used_bits};
	}

	return {src_size, dst_index*8};
}

std::pair<size_t, size_t> HuffmanDictionary::decode(const char* src, size_t src_size, char* dst, size_t dst_size, size_t src_offset_start, size_t src_offset_end)
{
	if(!this->is_initialized())
	{
		return {0, 0};
	}

	if(src_offset_end != 0 && src_size != 0)
	{
		src_size--;
	}

	size_t src_index = src_offset_start / 8;
	size_t bits_left = src_offset_start % 8;
	size_t dst_index = 0;
	char byte = src[src_index] >> (8 - bits_left);
	const huffman_tree_node* current_node = this->m_root.get();

	while(true)
	{
		while(bits_left)
		{
			if(!current_node->is_character())
			{
				if(byte & 1)
				{
					current_node = current_node->m_left.get();
				}
				else
				{
					current_node = current_node->m_right.get();
				}

				bits_left--;
				byte >>= 1;
			}

			if(current_node->is_character())
			{
				if(dst_index >= dst_size)
				{
					return {src_index*8+(8-bits_left), dst_index};
				}

				dst[dst_index++] = current_node->m_character;
				current_node = this->m_root.get();
			}
		}

		if(src_index >= src_size)
		{
			if(src_offset_end == 0)
			{
				return {src_index*8+(8-bits_left), dst_index};
			}
			else
			{
				byte = src[src_index++];
				bits_left = src_offset_end;
				src_offset_end = 0;
			}
		}
		else
		{
			byte = src[src_index++];
			bits_left = 8;
		}
	}
}