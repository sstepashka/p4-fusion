/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#pragma once

#include "common.h"

#include <memory>

class Result : public ClientUser
{
	bool m_IsError;
    bool m_IsFatal;

public:
	Result() = default;

	void HandleError(Error* e) override;

    bool IsError() const {
        return m_IsError;
    }

    bool IsFatal() const {
        return m_IsFatal;
    }

	Result(const Result&) = delete;
	Result& operator=(const Result&) = delete;

	Result(Result&&) = delete;
	Result& operator=(Result&&) = delete;
};
