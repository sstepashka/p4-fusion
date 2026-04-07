/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "print_result.h"

#include <iostream>

#include <p4/errornum.h>
#include <p4/msgdm.h>
#include <p4/msglbr.h>
#include <p4/msgsupp.h>

void PrintResult::OutputStat(StrDict* varList)
{
	m_Data.push_back(PrintData {});
}

void PrintResult::OutputText(const char* data, int length)
{
	std::vector<char>& fileContent = m_Data.back().contents;
	fileContent.insert(fileContent.end(), data, data + length);
}

void PrintResult::HandleError(Error* e) {
    if (e->CheckIds(MsgLbr::LbrOpenFail) || e->CheckIds(MsgSupp::MagicHeader)) {
        StrBuf str;
	    e->Fmt(&str);
        char* text = str.Text();
        ERR("Detected: Error opening librarian file (skipping the file): " << e->FmtSeverity() << " " << text);

        std::vector<char>& fileContent = m_Data.back().contents;
        fileContent.assign(text, text + std::strlen(text));

        return;
    }

    Result::HandleError(e);
}

void PrintResult::OutputBinary(const char* data, int length)
{
	OutputText(data, length);
}
