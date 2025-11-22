/****************************************************************************
 *
 *   Copyright (c) 2023 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#pragma once

#include "ParamLayer.h"

#include <px4_platform_common/atomic.h>

class DynamicSparseLayer : public ParamLayer
{
public:
	DynamicSparseLayer(ParamLayer *parent, int n_prealloc = 32, int n_grow = 4) : ParamLayer(parent),
		_n_prealloc(n_prealloc), _n_slots(0), _n_grow(n_grow)
	{
		// SAMV7 FIX: Lazy allocation - defer malloc() until first use
		// On SAMV7, malloc() fails during C++ static initialization phase
		// because the heap is not ready yet. Deferring allocation to runtime
		// when heap is available fixes "param set" and "param save" failures.
		_slots.store(nullptr);
	}

	virtual ~DynamicSparseLayer()
	{
		if (_slots.load()) {
			free(_slots.load());
		}
	}

	bool store(param_t param, param_value_u value) override
	{
		AtomicTransaction transaction;

		// SAMV7 FIX: Ensure allocation before accessing slots
		if (!_ensure_allocated()) {
			return false;
		}

		Slot *slots = _slots.load();

		const int index = _getIndex(param);

		if (index < _next_slot) { // already exists
			slots[index].value = value;

		} else if (_next_slot < _n_slots) {
			slots[_next_slot++] = {param, value};
			_sort();

		} else {
			if (!_grow(transaction)) {
				return false;
			}

			_slots.load()[_next_slot++] = {param, value};
			_sort();
		}

		return true;
	}

	bool contains(param_t param) const override
	{
		const AtomicTransaction transaction;

		// SAMV7 FIX: If not allocated, we don't contain anything
		if (!_ensure_allocated()) {
			return false;
		}

		return _getIndex(param) < _next_slot;
	}

	px4::AtomicBitset<PARAM_COUNT> containedAsBitset() const override
	{
		px4::AtomicBitset<PARAM_COUNT> set;
		const AtomicTransaction transaction;

		// SAMV7 FIX: If not allocated, return empty bitset
		if (!_ensure_allocated()) {
			return set;
		}

		Slot *slots = _slots.load();

		for (int i = 0; i < _next_slot; i++) {
			set.set(slots[i].param);
		}

		return set;
	}

	param_value_u get(param_t param) const override
	{
		const AtomicTransaction transaction;

		// SAMV7 FIX: If not allocated yet, fall back to parent or firmware defaults
		if (!_ensure_allocated()) {
			if (_parent == nullptr) {
				return px4::parameters[param].val;
			}

			return _parent->get(param);
		}

		Slot *slots = _slots.load();

		const int index = _getIndex(param);

		if (index < _next_slot) { // exists in our data structure
			return slots[index].value;
		}

		// Workaround for C++ static initialization bug on SAMV7
		// If parent is null, return default value instead of crashing
		if (_parent == nullptr) {
			return px4::parameters[param].val;
		}

		return _parent->get(param);
	}

	void reset(param_t param) override
	{
		const AtomicTransaction transaction;

		// SAMV7 FIX: If not allocated, nothing to reset
		if (!_ensure_allocated()) {
			return;
		}

		int index = _getIndex(param);
		Slot *slots = _slots.load();

		if (index < _next_slot) {
			slots[index] = {UINT16_MAX, param_value_u{}};
			_sort();
			_next_slot--;
		}
	}

	void refresh(param_t param) override
	{
		_parent->refresh(param);
	}

	int size() const override
	{
		return _next_slot;
	}

	int byteSize() const override
	{
		return _n_slots * sizeof(Slot);
	}

private:
	struct Slot {
		param_t param;
		param_value_u value;
	};

	// SAMV7 FIX: Lazy allocation helper
	// On SAMV7, malloc() fails during C++ static initialization.
	// This method defers allocation until first use when heap is ready.
	bool _ensure_allocated() const
	{
		// Fast path: already allocated
		if (_slots.load() != nullptr) {
			return true;
		}

		// Lazy allocation on first use (heap now available)
		Slot *slots = (Slot *)malloc(sizeof(Slot) * _n_prealloc);

		if (slots == nullptr) {
			PX4_ERR("Failed to allocate memory for dynamic sparse layer (lazy)");
			return false;
		}

		// Initialize slots
		for (int i = 0; i < _n_prealloc; i++) {
			slots[i] = {UINT16_MAX, param_value_u{}};
		}

		// Atomic compare-exchange for thread safety
		// Only one thread wins the race; losers free their allocation
		Slot *expected = nullptr;

		if (!const_cast<px4::atomic<Slot *>&>(_slots).compare_exchange(&expected, slots)) {
			// Another thread won the race, free our allocation
			free(slots);
		} else {
			// We won the race, update slot count
			const_cast<int &>(_n_slots) = _n_prealloc;
		}

		return true;
	}

	static int _slotCompare(const void *a, const void *b)
	{
		return ((int)((Slot *)a)->param) - ((int)((Slot *)b)->param);
	}

	void _sort()
	{
		qsort(_slots.load(), _n_slots, sizeof(Slot), _slotCompare);
	}

	int _getIndex(param_t param) const
	{
		int left = 0;
		int right = _next_slot - 1;
		Slot *slots = _slots.load();

		while (left <= right) {
			int mid = (left + right) / 2;

			if (slots[mid].param == param) {
				return mid;

			} else if (slots[mid].param < param) {
				left = mid + 1;

			} else {
				right = mid - 1;
			}
		}

		return _next_slot;
	}

	bool _grow(AtomicTransaction &transaction)
	{
		if (_n_slots == 0) {
			return false;
		}

		int max_retries = 5;

		// As malloc uses locking, so we need to re-enable IRQ's during malloc/free and
		// then atomically exchange the buffer
		while (_next_slot >= _n_slots && max_retries-- > 0) {
			Slot *previous_slots = nullptr;
			Slot *new_slots = nullptr;

			do {
				previous_slots = _slots.load();
				transaction.unlock();

				if (new_slots) {
					free(new_slots);
				}

				new_slots = (Slot *) malloc(sizeof(Slot) * (_n_slots + _n_grow));
				transaction.lock();

				if (new_slots == nullptr) {
					return false;
				}

			} while (!_slots.compare_exchange(&previous_slots, new_slots));

			memcpy(new_slots, previous_slots, sizeof(Slot) * _n_slots);

			for (int i = _n_slots; i < _n_slots + _n_grow; i++) {
				new_slots[i] = {UINT16_MAX, param_value_u{}};
			}

			_n_slots += _n_grow;

			transaction.unlock();
			free(previous_slots);
			transaction.lock();
		}

		return _next_slot < _n_slots;
	}

	int _next_slot = 0;
	const int _n_prealloc;  // SAMV7 FIX: Remember prealloc size for lazy init
	int _n_slots = 0;
	const int _n_grow;
	px4::atomic<Slot *> _slots{nullptr};
};
