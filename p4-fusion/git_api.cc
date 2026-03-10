/*
 * Copyright (c) 2022 Salesforce, Inc.
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 * For full license text, see the LICENSE.txt file in the repo root or https://opensource.org/licenses/BSD-3-Clause
 */
#include "git_api.h"

#include <cstring>
#include <cstdlib>

#include "git2.h"
#include "git2/index.h"
#include "git2/sys/repository.h"
#include "minitrace.h"
#include "utils/std_helpers.h"

namespace {
void git2(int x, const std::string& func, const std::string& file, int line) {
	int error = x;
	if (error < 0)
	{
		const git_error* e = git_error_last();
		std::cerr << Log::ColorRed << "[ ERROR @ " << file << ":" << func << ":" << line << " ]\n";
		std::cerr << "  Error code: " << error << "\n";
		if (e) {
			std::cerr << "  Error class: " << e->klass << " (";
			switch (e->klass) {
				case 0: std::cerr << "GIT_ERROR_NONE"; break;
				case 1: std::cerr << "GIT_ERROR_NOMEMORY"; break;
				case 2: std::cerr << "GIT_ERROR_OS"; break;
				case 3: std::cerr << "GIT_ERROR_INVALID"; break;
				case 4: std::cerr << "GIT_ERROR_REFERENCE"; break;
				case 5: std::cerr << "GIT_ERROR_ZLIB"; break;
				case 6: std::cerr << "GIT_ERROR_REPOSITORY"; break;
				case 7: std::cerr << "GIT_ERROR_CONFIG"; break;
				case 8: std::cerr << "GIT_ERROR_REGEX"; break;
				case 9: std::cerr << "GIT_ERROR_ODB"; break;
				case 10: std::cerr << "GIT_ERROR_INDEX"; break;
				case 11: std::cerr << "GIT_ERROR_OBJECT"; break;
				case 12: std::cerr << "GIT_ERROR_NET"; break;
				case 13: std::cerr << "GIT_ERROR_TAG"; break;
				case 14: std::cerr << "GIT_ERROR_TREE"; break;
				case 15: std::cerr << "GIT_ERROR_INDEXER"; break;
				case 16: std::cerr << "GIT_ERROR_SSL"; break;
				case 17: std::cerr << "GIT_ERROR_SUBMODULE"; break;
				case 18: std::cerr << "GIT_ERROR_THREAD"; break;
				case 19: std::cerr << "GIT_ERROR_STASH"; break;
				case 20: std::cerr << "GIT_ERROR_CHECKOUT"; break;
				case 21: std::cerr << "GIT_ERROR_FETCHHEAD"; break;
				case 22: std::cerr << "GIT_ERROR_MERGE"; break;
				case 23: std::cerr << "GIT_ERROR_SSH"; break;
				case 24: std::cerr << "GIT_ERROR_FILTER"; break;
				case 25: std::cerr << "GIT_ERROR_REVERT"; break;
				case 26: std::cerr << "GIT_ERROR_CALLBACK"; break;
				case 27: std::cerr << "GIT_ERROR_CHERRYPICK"; break;
				case 28: std::cerr << "GIT_ERROR_DESCRIBE"; break;
				case 29: std::cerr << "GIT_ERROR_REBASE"; break;
				case 30: std::cerr << "GIT_ERROR_FILESYSTEM"; break;
				case 31: std::cerr << "GIT_ERROR_PATCH"; break;
				case 32: std::cerr << "GIT_ERROR_WORKTREE"; break;
				case 33: std::cerr << "GIT_ERROR_SHA1"; break;
				case 34: std::cerr << "GIT_ERROR_HTTP"; break;
				default: std::cerr << "UNKNOWN"; break;
			}
			std::cerr << ")\n";
			std::cerr << "  Error message: " << e->message << "\n";
		} else {
			std::cerr << "  No git_error_last() available\n";
		}
		std::cerr << Log::ColorNormal;
        std::cerr << std::endl;
		throw std::runtime_error("libgit2 error: " + std::string(e ? e->message : "unknown error"));
	}
}
}

#define GIT2(x) git2(x, __func__, __FILE__, __LINE__)

GitAPI::GitAPI(bool fsyncEnable)
{
	git_libgit2_init();

	GIT2(git_libgit2_opts(GIT_OPT_ENABLE_FSYNC_GITDIR, (int)fsyncEnable));

	// Since we trust the hard-drive and operating system, we can skip the verification.
	GIT2(git_libgit2_opts(GIT_OPT_ENABLE_STRICT_HASH_VERIFICATION, (int)0));

	// Global RAM cache: 1gb...
	GIT2(git_libgit2_opts(GIT_OPT_SET_CACHE_MAX_SIZE, (ssize_t)(1024 * 1024 * 1024)));

	// 20Mb for the file name tree cache...
	GIT2(git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJECT_TREE, (size_t)(20 * 1024 * 1024)));

	// 20Mb for commit info cache...
	GIT2(git_libgit2_opts(GIT_OPT_SET_CACHE_OBJECT_LIMIT, GIT_OBJECT_COMMIT, (size_t)(20 * 1024 * 1024)));
}

GitAPI::~GitAPI()
{
	if (m_Repo)
	{
		git_repository_free(m_Repo);
		m_Repo = nullptr;
	}
	git_libgit2_shutdown();
}

bool GitAPI::IsRepositoryClonedFrom(const std::string& depotPath)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	git_oid oid;
	GIT2(git_reference_name_to_id(&oid, m_Repo, "HEAD"));

	git_commit* headCommit = nullptr;
	GIT2(git_commit_lookup(&headCommit, m_Repo, &oid));

	std::string message = git_commit_message(headCommit);
	size_t depotPathStart = message.find("depot-paths = \"") + 15;
	size_t depotPathEnd = message.find("\": change") - 1;
	std::string repoDepotPath = message.substr(depotPathStart, depotPathEnd - depotPathStart + 1) + "...";

	git_commit_free(headCommit);

	return repoDepotPath == depotPath;
}

void GitAPI::OpenRepository(const std::string& repoPath)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	GIT2(git_repository_open(&m_Repo, repoPath.c_str()));
}

bool GitAPI::InitializeRepository(const std::string& srcPath)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	GIT2(git_repository_init(&m_Repo, srcPath.c_str(), true));
	SUCCESS("Initialized Git repository at " << srcPath);

	return true;
}

bool GitAPI::IsHEADExists()
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	git_oid oid;
	int errorCode = git_reference_name_to_id(&oid, m_Repo, "HEAD");
	if (errorCode && errorCode != GIT_ENOTFOUND)
	{
		GIT2(errorCode);
	}
	return errorCode == 0;
}

void GitAPI::SetActiveBranch(const std::string& branchName)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	if (branchName == m_CurrentBranch)
	{
		return;
	}

	int errorCode;
	// Look up the branch.
	git_reference* branch;
	errorCode = git_reference_lookup(&branch, m_Repo, ("refs/heads/" + branchName).c_str());
	if (errorCode != 0 && errorCode != GIT_ENOTFOUND)
	{
		GIT2(errorCode);
	}
	if (errorCode == GIT_ENOTFOUND)
	{
		// Create the branch from the first index.
		git_commit* firstCommit = nullptr;
		GIT2(git_commit_lookup(&firstCommit, m_Repo, &m_FirstCommitOid));
		GIT2(git_branch_create(&branch, m_Repo, branchName.c_str(), firstCommit, 0));
		git_commit_free(firstCommit);
	}

	// Make head point to the branch.
	git_reference* head;
	GIT2(git_reference_symbolic_create(&head, m_Repo, "HEAD", git_reference_name(branch), 1, branchName.c_str()));
	git_reference_free(head);
	git_reference_free(branch);

	// Now update the files in the index to match the content of the commit pointed at by HEAD.
	git_oid oidParentCommit;
	GIT2(git_reference_name_to_id(&oidParentCommit, m_Repo, "HEAD"));

	git_commit* headCommit = nullptr;
	GIT2(git_commit_lookup(&headCommit, m_Repo, &oidParentCommit));

	git_tree* headCommitTree = nullptr;
	GIT2(git_commit_tree(&headCommitTree, headCommit));

	GIT2(git_index_read_tree(m_Index, headCommitTree));

	git_tree_free(headCommitTree);
	git_commit_free(headCommit);

	m_CurrentBranch = branchName;
}

git_oid GitAPI::CreateBlob(const std::vector<char>& data)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	git_oid oid;
	GIT2(git_blob_create_from_buffer(&oid, m_Repo, data.data(), data.size()));
	return oid;
}

std::string GitAPI::DetectLatestCL()
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	git_oid oid;
	GIT2(git_reference_name_to_id(&oid, m_Repo, "HEAD"));

	git_commit* headCommit = nullptr;
	GIT2(git_commit_lookup(&headCommit, m_Repo, &oid));

	std::string message = git_commit_message(headCommit);
	// Look for the specific change message generated from the Commit method.
	// Note that extra branching information can be added after it.
	// ": change = " is 11 characters long.
	size_t clStart = message.rfind(": change = ") + 11;
	size_t clEnd = message.find(']', clStart);
	std::string cl(message, clStart, clEnd - clStart);

	git_commit_free(headCommit);

	return cl;
}

void GitAPI::CreateIndex()
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	MTR_SCOPE("Git", __func__);

	GIT2(git_repository_index(&m_Index, m_Repo));

	if (IsHEADExists())
	{
		git_oid oid_parent_commit = {};
		GIT2(git_reference_name_to_id(&oid_parent_commit, m_Repo, "HEAD"));

		git_commit* head_commit = nullptr;
		GIT2(git_commit_lookup(&head_commit, m_Repo, &oid_parent_commit));

		git_tree* head_commit_tree = nullptr;
		GIT2(git_commit_tree(&head_commit_tree, head_commit));

		GIT2(git_index_read_tree(m_Index, head_commit_tree));

		git_tree_free(head_commit_tree);
		git_commit_free(head_commit);

		// Find the first commit
		git_revwalk* walk;
		git_revwalk_new(&walk, m_Repo);
		git_revwalk_sorting(walk, GIT_SORT_TOPOLOGICAL);
		git_revwalk_push_head(walk);
		git_revwalk_next(&m_FirstCommitOid, walk);
		git_revwalk_free(walk);

		WARN("Loaded index was refreshed to match the tree of the current HEAD commit");
	}
	else
	{
		// In order to have branches be mergable, even with no shared history, we perform
		// a trick by adding an empty commit as the very first commit, and use this as the base for all branches.
		// The time is set to the beginning of time.
		git_oid commitTreeID;
		GIT2(git_index_write_tree_to(&commitTreeID, m_Index, m_Repo));

		git_tree* commitTree = nullptr;
		GIT2(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

		git_signature* author = nullptr;
		GIT2(git_signature_new(&author, "No User", "no@user", 0, 0));

		git_reference* ref = nullptr;
		git_object* parent = nullptr;
		git_revparse_ext(&parent, &ref, m_Repo, "HEAD");

		GIT2(git_commit_create_v(&m_FirstCommitOid, m_Repo, "HEAD", author, author, "UTF-8", "Initial repository.", commitTree, parent ? 1 : 0, parent));

		git_object_free(parent);
		git_reference_free(ref);
		git_signature_free(author);
		git_tree_free(commitTree);

		WARN("No HEAD commit was found. Created fresh index " << git_oid_tostr_s(&m_FirstCommitOid) << ".");
	}
}

void GitAPI::AddFileToIndex(const std::string& relativePath, git_oid contents, const bool plusx) {
    const std::lock_guard<std::recursive_mutex> guard{m_};
	MTR_SCOPE("Git", __func__);

	git_index_entry entry = {};
    entry.id = contents;
	entry.mode = GIT_FILEMODE_BLOB;
	if (plusx)
	{
		entry.mode = GIT_FILEMODE_BLOB_EXECUTABLE; // 0100755
	}

	entry.path = relativePath.c_str();
    GIT2(git_index_add(m_Index, &entry));
}

void GitAPI::RemoveFileFromIndex(const std::string& relativePath)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	MTR_SCOPE("Git", __func__);

	GIT2(git_index_remove_bypath(m_Index, relativePath.c_str()));
}

std::string GitAPI::Commit(
    const std::string& depotPath,
    const std::string& cl,
    const std::string& user,
    const std::string& email,
    const int& timezone,
    std::string desc,
    const int64_t& timestamp,
    const std::string& mergeFromStream)
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	MTR_SCOPE("Git", __func__);

	// Debug: log all index entries before writing tree
	size_t entryCount = git_index_entrycount(m_Index);
	std::cerr << "Index contains " << entryCount << " entries before writing tree\n";
	for (size_t i = 0; i < entryCount; i++) {
		const git_index_entry* entry = git_index_get_byindex(m_Index, i);
		if (entry) {
			std::string path = entry->path;
			std::cerr << "  [" << i << "] path='" << path << "' mode=" << entry->mode << "\n";
			// Check for invalid characters
			if (path.empty()) {
				std::cerr << "    WARNING: Empty path!\n";
			}
			if (path.find("//") != std::string::npos) {
				std::cerr << "    WARNING: Double slashes in path!\n";
			}
			if (path[0] == '/') {
				std::cerr << "    WARNING: Path starts with slash!\n";
			}
			if (path.back() == '/') {
				std::cerr << "    WARNING: Path ends with slash!\n";
			}
			if (path.find('\0') != std::string::npos) {
				std::cerr << "    WARNING: Null byte in path!\n";
			}
			// Check for invalid path components
			size_t pos = 0;
			while ((pos = path.find('/', pos)) != std::string::npos) {
				if (pos > 0 && path.substr(pos - 1, 1) == ".") {
					if (pos == 1 || (pos > 1 && path.substr(pos - 2, 2) == "/.")) {
						std::cerr << "    WARNING: Path contains '/./' or starts with './'\n";
					}
				}
				pos++;
			}
		}
	}

	git_oid commitTreeID;
	GIT2(git_index_write_tree_to(&commitTreeID, m_Index, m_Repo));

	git_tree* commitTree = nullptr;
	GIT2(git_tree_lookup(&commitTree, m_Repo, &commitTreeID));

	git_signature* author = nullptr;
	GIT2(git_signature_new(&author, user.c_str(), email.c_str(), timestamp, timezone));

	STDHelpers::StripSurrounding(desc, '\n');
	STDHelpers::StripSurrounding(desc, '\r');
	STDHelpers::StripSurrounding(desc, ' ');
	STDHelpers::StripSurrounding(desc, '\t');

	// -3 to remove the trailing "..."
	std::string commitMsg = cl + " - " + desc + "\n[p4-fusion: depot-paths = \"" + depotPath.substr(0, depotPath.size() - 3) + "\": change = " + cl + "]";

	// Find the parent commits.
	// Order is very important.
	std::vector<std::string> parentRefs = { "HEAD" };
	if (!mergeFromStream.empty())
	{
		parentRefs.push_back("refs/heads/" + mergeFromStream);
	}

	git_commit* parents[2];
	int parentCount = 0;
	for (std::string& parentRef : parentRefs)
	{
		git_oid refOid;
		int errorCode = git_reference_name_to_id(&refOid, m_Repo, parentRef.c_str());
		if (errorCode != 0 && errorCode != GIT_ENOTFOUND)
		{
			GIT2(errorCode);
		}
		else if (errorCode != GIT_ENOTFOUND)
		{
			git_commit* refCommit = nullptr;
			GIT2(git_commit_lookup(&refCommit, m_Repo, &refOid));
			if (parentCount > 0)
			{
				commitMsg += "; merged from " + parentRef;
			}
			parents[parentCount++] = refCommit;
		}
		// Skip the reference if it wasn't found.  That means it doesn't
		// exist yet, which means there wasn't a previous commit for it.
		// Practically speaking, this should only happen in non-branching
		// mode on the first commit.
	}

	git_oid commitID;
	GIT2(git_commit_create(&commitID, m_Repo, "HEAD", author, author, "UTF-8", commitMsg.c_str(), commitTree, parentCount, (const git_commit**)parents));

	for (int i = 0; i < parentCount; i++)
	{
		git_commit_free(parents[i]);
		parents[i] = nullptr;
	}
	git_signature_free(author);
	git_tree_free(commitTree);

	return git_oid_tostr_s(&commitID);
}

void GitAPI::CloseIndex()
{
    const std::lock_guard<std::recursive_mutex> guard{m_};
	GIT2(git_index_write(m_Index));
	git_index_free(m_Index);
}
