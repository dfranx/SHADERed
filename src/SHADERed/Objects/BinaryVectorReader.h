#pragma once
#include <spvgentwo/Reader.h>

namespace ed {
	template <typename U32Vector>
	class BinaryVectorReader : public spvgentwo::IReader {
	public:
		BinaryVectorReader(U32Vector& spv)
				: m_vec(spv)
				, m_index(0)
		{
		}
		~BinaryVectorReader() { }

		bool get(unsigned int& _word) final
		{
			if (m_index >= m_vec.size())
				return false;
			_word = m_vec[m_index++];
			return true;
		}

	private:
		U32Vector& m_vec;
		int m_index;
	};
}