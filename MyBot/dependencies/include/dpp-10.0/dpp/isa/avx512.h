/************************************************************************************
 *
 * D++, A Lightweight C++ library for Discord
 *
 * Copyright 2021 Craig Edwards and D++ contributors
 * (https://github.com/brainboxdotcc/DPP/graphs/contributors)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ************************************************************************************/
#pragma once

#if defined _MSC_VER || defined __GNUC__ || defined __clang__

#include <immintrin.h>

#ifdef max
	#undef max
#endif
#ifdef min
	#undef min
#endif


namespace dpp {

	using avx_512_float = __m512;
	using avx_512_int = __m512i;

	/*
	* @brief Extracts a 32-bit integer from a 512-bit AVX-512 register.
	* @param value The AVX-512 register containing packed 32-bit integers.
	* @param index The index of the 32-bit integer to extract (0-15).
	* @return The extracted 32-bit integer.
	*/
	inline int32_t extract_int32_from_avx512(const avx_512_int& value, int64_t index) {
		alignas(64) int32_t result[32];
		_mm512_store_si512(result, value);
		return result[index];
	}

	/**
	 * @brief A class for audio mixing operations using AVX512 instructions.
	 */
	class audio_mixer {
	public:
		/*
		 * @brief The number of 32-bit values per CPU register.
		 */
		inline static constexpr int32_t byte_blocks_per_register{ 16 };

		/*
		 * @brief Stores values from a 512-bit AVX512 vector to a storage location.
		 * @tparam value_type The target value type for storage.
		 * @param values_to_store The 512-bit AVX512 vector containing values to store.
		 * @param storage_location Pointer to the storage location.
		 */
		template<typename value_type> inline static void store_values(const avx_512_int& values_to_store, value_type* storage_location) {
			for (int64_t x = 0; x < byte_blocks_per_register; ++x) {
				storage_location[x] = static_cast<value_type>(extract_int32_from_avx512(values_to_store, x));
			}
		}

		/**
		 * @brief Specialization for gathering non-float values into an AVX512 register.
		 * @tparam value_type The type of values being gathered.
		 * @tparam Indices Parameter pack of indices for gathering values.
		 * @return An AVX512 register containing gathered values.
		 */
		template<typename value_type> inline static avx_512_float gather_values(value_type* values) {
			alignas(64) float new_array[byte_blocks_per_register]{};
			for (size_t x = 0; x < byte_blocks_per_register; ++x) {
				new_array[x] = static_cast<float>(values[x]);
			}
			return _mm512_load_ps(new_array);
		}

		/**
		 * @brief Collect a single register worth of data from data_in, apply gain and increment, and store the result in data_out.
		 *        This version uses AVX512 instructions.
		 *
		 * @param data_in Pointer to the input array of int32_t values.
		 * @param data_out Pointer to the output array of int16_t values.
		 * @param current_gain The gain to be applied to the elements.
		 * @param increment The increment value to be added to each element.
		 */
		inline static void collect_single_register(int32_t* data_in, int16_t* data_out, float current_gain, float increment) {
			avx_512_float current_samples_new{ _mm512_mul_ps(gather_values(data_in),
				_mm512_add_ps(_mm512_set1_ps(current_gain),
					_mm512_mul_ps(_mm512_set1_ps(increment),
						_mm512_set_ps(0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f)))) };

			__m512 lower_limit = _mm512_set1_ps(static_cast<float>(std::numeric_limits<int16_t>::min()));
			__m512 upper_limit = _mm512_set1_ps(static_cast<float>(std::numeric_limits<int16_t>::max()));

			__mmask16 mask_ge = _mm512_cmp_ps_mask(current_samples_new, _mm512_set1_ps(0.0f), _CMP_GE_OQ);

			current_samples_new = _mm512_mask_max_ps(current_samples_new, mask_ge, current_samples_new, lower_limit);
			current_samples_new = _mm512_mask_min_ps(current_samples_new, ~mask_ge, current_samples_new, upper_limit);

			store_values(_mm512_cvtps_epi32(current_samples_new), data_out);
		}

		/**
		 * @brief Combine a register worth of elements from decoded_data and store the result in up_sampled_vector.
		 *        This version uses AVX512 instructions.
		 *
		 * @param up_sampled_vector Pointer to the array of int32_t values.
		 * @param decoded_data Pointer to the array of int16_t values.
		 */
		inline static void combine_samples(int32_t* up_sampled_vector, const int16_t* decoded_data) {
			auto newValues{ _mm512_cvtps_epi32(_mm512_add_ps(gather_values(up_sampled_vector), gather_values(decoded_data))) };
			store_values(newValues, up_sampled_vector);
		}
	};

} // namespace dpp

#endif