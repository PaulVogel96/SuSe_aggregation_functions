#include "summary_selector.hpp"

#include <boost/multiprecision/cpp_int.hpp>

#include <doctest/doctest.h>

TEST_SUITE("suse::summary_selector")
{
	TEST_CASE("simple, irrelevant time window")
	{
		suse::summary_selector<int> selector("a(b|c)d?e",10,10);

		for(std::size_t idx=0; auto c: "abcde")
			selector.process_event({c, idx++, 0});

		CHECK(selector.number_of_contained_complete_matches()==4);
	}
	
	TEST_CASE("slightly less simple, irrelevant time window")
	{
		suse::summary_selector<int> selector("a(b|c)+d?e",10,10);

		for(std::size_t idx=0; auto c: "aabcde")
			selector.process_event({c, idx++, 0});

		CHECK(selector.number_of_contained_complete_matches()==12);
	}
	
	TEST_CASE("larger test, irrelevant time window")
	{
		const std::string_view input = "aabbcbcbcacbccbabcdeacbdeabdadedbcbcdacdeacbdacaaacbcabcdbcacbcdacbadcbacdbcacbdabdacbdacbcbcecbacbbcbbcbbcacbdcabcaaaaabddccbacbcasdfcbbedacccadcbcbcdabdbacdabdacbcbcbcbabcecbadcbecbacbdcbacbcbabdabjaljadcadcaddcdaaaaabceaaabaeeasdwbdceaidaadcdeaaaacdesdcasdefaserdabcedsiabfaasefbdedaaddccdeafceabdebdeadaaabeadabdasdfbcadcdfabeaderasdfcbede";
		
		suse::summary_selector<boost::multiprecision::cpp_int> selector("a(b|c)+d?e",input.size(),input.size());
		for(std::size_t idx=0; auto c: input)
			selector.process_event({c, idx++, 0});

		CHECK(selector.number_of_contained_complete_matches()==boost::multiprecision::cpp_int{"513920788981887762029269814291248651974076116"});
	}
	
	TEST_CASE("larger test, relevant time window")
	{
//		using int_type = boost::multiprecision::cpp_int;
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "aabbcbcbcacbccbabcdeacbdeabdadedbcbcdacdeacbdacaaacbcabcdbcacbcdacbadcbacdbcacbdabdacbdacbcbcecbacbbcbbcbbcacbdcabcaaaaabddccbacbcadcbbedacccadcbcbcdabdbacdabdacbcbcbcbabcecbadcbecbacbdcbacbcbabdabaadcadcaddcdaaaaabceaaabaeeadbdceadaadcdeaaaacdedcadeaedabcedabaaebdedaaddccdeaceabdebdeadaaabeadabdadbcadcdabeadeadcbedeaabbcbcbcacbccbabcdeacbdeabdadedbcbcdacdeacbdacaaacbcabcdbcacbcdacbadcbacdbcacbdabdacbdacbcbcecbacbbcbbcbbcacbdcabcaaaaabddccbacbcadcbbedacccadcbcbcdabdbacdabdacbcbcbcbabcecbadcbecbacbdcbacbcbabdabaadcadcaddcdaaaaabceaaabaeeadbdceadaadcdeaaaacdedcadeaedabcedabaaebdedaaddccdeaceabdebdeadaaabeadabdadbcadcdabeadeadcbede";

		suse::summary_selector<int_type> selector("a*b(c|d)*e",input.size(),253);
		for(std::size_t idx=0; auto c: input)
			selector.process_event({c, idx++, 0});

		CHECK(selector.number_of_contained_complete_matches()==int_type{"133190355408367558081362438558651296"});
	}
	
	TEST_CASE("remove")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "aabcdacbadcbacdbcacbdaedabaaebdedaaddccdeaceabdebdeadaaabeadabdadbcadcdabeadeadcbedeaabbcbcbcacbccbabcdeacbdeabdadedbcbcdacdeacbdacaaacbcabcdbcacbcdacbadcbacdbcacbdabdacbdacbcbcecbacbbcbbcbbcacbdcabcaaaaabddccbacbcadcbbedacccadcbcbcdabdbacdabdacbcbcbcbabcecbadcbecbacbdcbacbcbabdabaadcadcaddcdaaaaabceaaabaeedeaedabcedabaaebdedaadde";

		for(std::size_t to_delete=0;to_delete<input.size();++to_delete)
		{
			CAPTURE(to_delete);

			suse::summary_selector<int_type> correct_selector("a*b(c|d)+e",input.size(),42);
			suse::summary_selector<int_type> selector("a*b(c|d)+e",input.size(),42);
			
			for(std::size_t idx=0; auto c: input)
			{
				if(idx!=to_delete)
					correct_selector.process_event({c, idx++, 0});
				else
					++idx;
			}
	
			for(std::size_t idx=0; auto c: input)
				selector.process_event({c, idx++, 0});
			selector.remove_event(to_delete);

			// Add one more element to align the timestamps/windows
			// otherwise the comparison fails when deleting the very last element
			// as adding and removing again moves the window and does therefore (correctly)
			// not produce the same state as doing nothing 
			correct_selector.process_event({'x', input.size()+10, 0});
			selector.process_event({'x', input.size()+10, 0});

			REQUIRE(selector==correct_selector);
		}
	}

	/* This Test tests the behavior of the selectors.remove_event()
	* method. Supposedly, all the selectors should be equal, regardless 
	* of if an event was removed or if it was skipped during the process.
	* There is still an error, as once the remove_event() method is called,
	* The total sum counters are not equal anymore. This means there must 
	* be a flaw in the removal logic. h
	*/
	TEST_CASE("remove madness")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "BBBABBCBCBBACABABCCCBBACBABBACABBBCDCBCCBAABBABACBADBDCBCBAABBACDABAACBACBADCBCDBBDBBACBABAACBDCABBBABBBACCBCBBCDAACCBBBBAABADACBABCAACAABBBACBBDACBBBCBACACBACBCBBDAABBBABBCDCACABACBABBBBBCCBBACACBBAAACCBBAAABBAAABCACCABCABABABCACBBABBCBABCBCCBCCCAACBAABCCDBDCCBAABCCABBCBBBBBCCBCBABCBBCABCCBABAAABABBBBBACBBBBBCABBCBBACCCAABBABCDCBADBAABAABBBBCDBBACCBCBBCBABCDBBBBBCCAAACCBBCBCBCCAABADBABAACBBDDACBBABCABABABADACCBBBBABBBBBBCBBCBACABDABACACDCBBABBCBDBBBBBBBBDCABAACBCBBBCABCBBACCCBBCCBCBABAAABADBBCBABABBCCBABBBABCACCBBACBBDBBCABCCADBCCBBCBBACCBCABAAACCABAACBBBBDCBDBCACBBBBAACCABBBBAABABCCBBCABABACBBDBACABBBBABCDABCAAAAACCBBBBCBABBDCABBDBCBBCBAAAABBBCBBBBBCBBBACCBBCCBBCDABCABDCABBBCCBCAADCBADAADBBBACBCCABBBCCCAACCBBBBCBBBBACBABABBABBCCAACCAAACBCCAACCBBBCDBDCBCACBBACBCBBCBBACAAABBBBDCBABBBCBDCACCBDBAAACBBACABABBBACCACCBBCBACDCCCBCDCBCDACBCBBCDBCBCACABBABCAABABDABBBBBBBCCCAAACBBACBCBCCABCAAABCBCBACABBBBCBDDBBBAACCBDBCCBABBBBBBBCABBACBABBCCCBAABBCDCBBBBCBCBACCCBBACCBBBCCBBBBCABCDBCACBCBCCCBDAA";
		const std::size_t to_delete = 498;

		suse::summary_selector<int_type> correct_selector("A(B*C)*D", 1000, 500);
		suse::summary_selector<int_type> selector("A(B*C)*D", 1000, 500);
		
		//std::cout << "Processing events for skipping selector" << std::endl;
		for(std::size_t idx = 0; auto c: input)
		{
			if (idx != to_delete) {
				//std::cout << "processing event " << c << "0" << std::endl;
				correct_selector.process_event({ c, idx++, 0 });
			}
			else {
				//std::cout << "skipping event " << c << " at index " << idx << std::endl;
				++idx;
			}
		}

		//std::cout << "Processing events for removal selector" << std::endl;
		for (std::size_t idx = 0; auto c: input) {
			selector.process_event({c, idx++, 0});
		}

		selector.remove_event(to_delete);

		REQUIRE(selector == correct_selector);
	}
	
	TEST_CASE("all hope abandon ye who enter here")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input_template = "BBBABBCBCBBACABABCCCBBACBABBACABBBCDCBCCBAABBABACBADBDCBCBAABBACDABAACBACBADCBCDBBDBBACBABAACBDCABBBABBBACCBCBBCDAACCBBBBAABADACBABCAACAABBBACBBDACBBBCBACACBACBCBBDAABBBABBCDCACABACBABBBBBCCBBACACBBAAACCBBAAABBAAABCACCABCABABABCACBBABBCBABCBCCBCCCAACBAABCCDBDCCBAABCCABBCBBBBBCCBCBABCBBCABCCBABAAABABBBBBACBBBBBCABBCBBACCCAABBABCDCBADBAABAABBBBCDBBACCBCBBCBABCDBBBBBCCAAACCBBCBCBCCAABADBABAACBBDDACBBABCABABABADACCBBBBABBBBBBCBBCBACABDABACACDCBBABBCBDBBBBBBBBDCABAACBCBBBCABCBBACCCBBCCBCBABAAABADBBCBABABBCCBABBBABCACCBBACBBDBBCABCCADBCCBBCBBACCBCABAAACCABAACBBBBDCBDBCACBBBBAACCABBBBAABABCCBBCABABACBBDBACABBBBABCDABCAAAAACCBBBBCBABBDCABBDBCBBCBAAAABBBCBBBBBCBBBACCBBCCBBCDABCABDCABBBCCBCAADCBADAADBBBACBCCABBBCCCAACCBBBBCBBBBACBABABBABBCCAACCAAACBCCAACCBBBCDBDCBCACBBACBCBBCBBACAAABBBBDCBABBBCBDCACCBDBAAACBBACABABBBACCACCBBCBACDCCCBCDCBCDACBCBBCDBCBCACABBABCAABABDABBBBBBBCCCAAACBBACBCBCCABCAAABCBCBACABBBBCBDDBBBAACCBDBCCBABBBBBBBCABBACBABBCCCBAABBCDCBBBBCBCBACCCBBACCBBBCCBBBBCABCDBCACBCBCCCBDAA";
		std::string input;
		for(int i=0;i<10;++i)
			input+=input_template;
		
		const std::size_t to_delete = 4998;

		suse::summary_selector<int_type> correct_selector("A(B*C)*D",10000,100);
		suse::summary_selector<int_type> selector("A(B*C)*D",10000,100);
		
		for(std::size_t idx=0; auto c: input)
		{
			if(idx!=to_delete)
				correct_selector.process_event({c, idx++, 0});
			else
				++idx;
		}

		for(std::size_t idx=0; auto c: input)
			selector.process_event({c, idx++, 0});
		selector.remove_event(to_delete);

		REQUIRE(selector==correct_selector);
	}
	
	TEST_CASE("remove a lot")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "BBBABBCBCBBACABABCCCBBACBABBACABBBCDCBCCBAABBABACBADBDCBCBAABBACDABAACBACBADCBCDBBDBBACBABAACBDCABBBABBBACCBCBBCDAACCBBBBAABADACBABCAACAABBBACBBDACBBBCBACACBACBCBBDAABBBABBCDCACABACBABBBBBCCBBACACBBAAACCBBAAABBAAABCACCABCABABABCACBBABBCBABCBCCBCCCAACBAABCCDBDCCBAABCCABBCBBBBBCCBCBABCBBCABCCBABAAABABBBBBACBBBBBCABBCBBACCCAABBABCDCBADBAABAABBBBCDBBACCBCBBCBABCDBBBBBCCAAACCBBCBCBCCAABADBABAACBBDDACBBABCABABABADACCBBBBABBBBBBCBBCBACABDABACACDCBBABBCBDBBBBBBBBDCABAACBCBBBCABCBBACCCBBCCBCBABAAABADBBCBABABBCCBABBBABCACCBBACBBDBBCABCCADBCCBBCBBACCBCABAAACCABAACBBBBDCBDBCACBBBBAACCABBBBAABABCCBBCABABACBBDBACABBBBABCDABCAAAAACCBBBBCBABBDCABBDBCBBCBAAAABBBCBBBBBCBBBACCBBCCBBCDABCABDCABBBCCBCAADCBADAADBBBACBCCABBBCCCAACCBBBBCBBBBACBABABBABBCCAACCAAACBCCAACCBBBCDBDCBCACBBACBCBBCBBACAAABBBBDCBABBBCBDCACCBDBAAACBBACABABBBACCACCBBCBACDCCCBCDCBCDACBCBBCDBCBCACABBABCAABABDABBBBBBBCCCAAACBBACBCBCCABCAAABCBCBACABBBBCBDDBBBAACCBDBCCBABBBBBBBCABBACBABBCCCBAABBCDCBBBBCBCBACCCBBACCBBBCCBBBBCABCDBCACBCBCCCBDAA";
		
		suse::summary_selector<int_type> correct_selector("A(B*C)*D",10000,100);
		suse::summary_selector<int_type> selector("A(B*C)*D",10000,100);

		for(std::size_t idx=0; auto c: input)
			selector.process_event({c, idx++, 0});

		for(std::size_t idx=0; auto c: input)
		{
			if(idx%2==1)
				correct_selector.process_event({c, idx++, 0});
			else
				++idx;
		}
		
		for(std::size_t i=0;i*2<input.size();++i)
			selector.remove_event(i);

		REQUIRE(selector==correct_selector);
	}
	
	TEST_CASE("remove everything")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "BBBABBCBCBBACABABCCCBBACBABBACABBBCDCBCCBAABBABACBADBDCBCBAABBACDABAACBACBADCBCDBBDBBACBABAACBDCABBBABBBACCBCBBCDAACCBBBBAABADACBABCAACAABBBACBBDACBBBCBACACBACBCBBDAABBBABBCDCACABACBABBBBBCCBBACACBBAAACCBBAAABBAAABCACCABCABABABCACBBABBCBABCBCCBCCCAACBAABCCDBDCCBAABCCABBCBBBBBCCBCBABCBBCABCCBABAAABABBBBBACBBBBBCABBCBBACCCAABBABCDCBADBAABAABBBBCDBBACCBCBBCBABCDBBBBBCCAAACCBBCBCBCCAABADBABAACBBDDACBBABCABABABADACCBBBBABBBBBBCBBCBACABDABACACDCBBABBCBDBBBBBBBBDCABAACBCBBBCABCBBACCCBBCCBCBABAAABADBBCBABABBCCBABBBABCACCBBACBBDBBCABCCADBCCBBCBBACCBCABAAACCABAACBBBBDCBDBCACBBBBAACCABBBBAABABCCBBCABABACBBDBACABBBBABCDABCAAAAACCBBBBCBABBDCABBDBCBBCBAAAABBBCBBBBBCBBBACCBBCCBBCDABCABDCABBBCCBCAADCBADAADBBBACBCCABBBCCCAACCBBBBCBBBBACBABABBABBCCAACCAAACBCCAACCBBBCDBDCBCACBBACBCBBCBBACAAABBBBDCBABBBCBDCACCBDBAAACBBACABABBBACCACCBBCBACDCCCBCDCBCDACBCBBCDBCBCACABBABCAABABDABBBBBBBCCCAAACBBACBCBCCABCAAABCBCBACABBBBCBDDBBBAACCBDBCCBABBBBBBBCABBACBABBCCCBAABBCDCBBBBCBCBACCCBBACCBBBCCBBBBCABCDBCACBCBCCCBDAA";
		
		suse::summary_selector<int_type> correct_selector("A(B*C)*D",input.size(),100);
		suse::summary_selector<int_type> selector("A(B*C)*D",input.size(),100);

		for(std::size_t idx=0; auto c: input)
			selector.process_event({c, idx++, 0});
		
		for(std::size_t i=0;i<input.size();++i)
			selector.remove_event(0);

		correct_selector.process_event({'x', input.size()+10, 0});
		selector.process_event({'x', input.size()+10, 0});

		REQUIRE(selector==correct_selector);
	}
	
	TEST_CASE("ttl")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "BBBABBCBCBBACABABCCCBBACBABBACABBBCDCBCCBAABBABACBADBDCBCBAABBACDABAACBACBADCBCDBBDBBACBABAACBDCABBBABBBACCBCBBCDAACCBBBBAABADACBABCAACAABBBACBBDACBBBCBACACBACBCBBDAABBBABBCDCACABACBABBBBBCCBBACACBBAAACCBBAAABBAAABCACCABCABABABCACBBABBCBABCBCCBCCCAACBAABCCDBDCCBAABCCABBCBBBBBCCBCBABCBBCABCCBABAAABABBBBBACBBBBBCABBCBBACCCAABBABCDCBADBAABAABBBBCDBBACCBCBBCBABCDBBBBBCCAAACCBBCBCBCCAABADBABAACBBDDACBBABCABABABADACCBBBBABBBBBBCBBCBACABDABACACDCBBABBCBDBBBBBBBBDCABAACBCBBBCABCBBACCCBBCCBCBABAAABADBBCBABABBCCBABBBABCACCBBACBBDBBCABCCADBCCBBCBBACCBCABAAACCABAACBBBBDCBDBCACBBBBAACCABBBBAABABCCBBCABABACBBDBACABBBBABCDABCAAAAACCBBBBCBABBDCABBDBCBBCBAAAABBBCBBBBBCBBBACCBBCCBBCDABCABDCABBBCCBCAADCBADAADBBBACBCCABBBCCCAACCBBBBCBBBBACBABABBABBCCAACCAAACBCCAACCBBBCDBDCBCACBBACBCBBCBBACAAABBBBDCBABBBCBDCACCBDBAAACBBACABABBBACCACCBBCBACDCCCBCDCBCDACBCBBCDBCBCACABBABCAABABDABBBBBBBCCCAAACBBACBCBCCABCAAABCBCBACABBBBCBDDBBBAACC";
		
		suse::summary_selector<int_type> correct_selector("A(B*C)*D",input.size()*2,100);
		suse::summary_selector<int_type> selector("A(B*C)*D",input.size()*2,100,input.size()*2);
		
		for(std::size_t idx=0; auto c: input)
		{
			selector.process_event({c, idx++, 0});
			correct_selector.process_event({c, idx++, 0});
		}

		selector.process_event({'a', input.size()*4, 0});
		correct_selector.process_event({'a', input.size()*4, 0});
	
		for(std::size_t i=0;i<input.size();++i)
			correct_selector.remove_event(0);

		REQUIRE(selector==correct_selector);
	}
	
	TEST_CASE("ttl - 2")
	{
		using int_type = boost::multiprecision::uint128_t;
		const std::string_view input = "BBBABBCBCBBACABABCCCBBACBABBACABBBCDCBCCBAABBABACBADBDCBCBAABBACDABAACBACBADCBCDBBDBBACBABAACBDCABBBABBBACCBCBBCDAACCBBBBAABADACBABCAACAABBBACBBDACBBBCBACACBACBCBBDAABBBABBCDCACABACBABBBBBCCBBACACBBAAACCBBAAABBAAABCACCABCABABABCACBBABBCBABCBCCBCCCAACBAABCCDBDCCBAABCCABBCBBBBBCCBCBABCBBCABCCBABAAABABBBBBACBBBBBCABBCBBACCCAABBABCDCBADBAABAABBBBCDBBACCBCBBCBABCDBBBBBCCAAACCBBCBCBCCAABADBABAACBBDDACBBABCABABABADACCBBBBABBBBBBCBBCBACABDABACACDCBBABBCBDBBBBBBBBDCABAACBCBBBCABCBBACCCBBCCBCBABAAABADBBCBABABBCCBABBBABCACCBBACBBDBBCABCCADBCCBBCBBACCBCABAAACCABAACBBBBDCBDBCACBBBBAACCABBBBAABABCCBBCABABACBBDBACABBBBABCDABCAAAAACCBBBBCBABBDCABBDBCBBCBAAAABBBCBBBBBCBBBACCBBCCBBCDABCABDCABBBCCBCAADCBADAADBBBACBCCABBBCCCAACCBBBBCBBBBACBABABBABBCCAACCAAACBCCAACCBBBCDBDCBCACBBACBCBBCBBACAAABBBBDCBABBBCBDCACCBDBAAACBBACABABBBACCACCBBCBACDCCCBCDCBCDACBCBBCDBCBCACABBABCAABABDABBBBBBBCCCAAACBBACBCBCCABCAAABCBCBACABBBBCBDDBBBAACC";
		
		suse::summary_selector<int_type> correct_selector("A(B*C)*D",input.size()*2,100);
		suse::summary_selector<int_type> selector("A(B*C)*D",input.size()*2,100,input.size()*2);
		
		for(std::size_t idx=0; auto c: input)
		{
			selector.process_event({c, idx++, 0});
			correct_selector.process_event({c, idx++, 0});
		}

		selector.process_event({'a', input.size()*2, 0});
		correct_selector.process_event({'a', input.size()*2, 0});

		selector.process_event({'a', input.size()*4, 0});
		correct_selector.process_event({'a', input.size()*4, 0});
	
		for(std::size_t i=0;i<input.size();++i)
			correct_selector.remove_event(0);

		REQUIRE(selector==correct_selector);
	}
}
