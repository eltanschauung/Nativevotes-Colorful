#if defined _nativevotes_progress_included
	#endinput
#endif
#define _nativevotes_progress_included

#define NATIVEVOTES_PROGRESS_NO_SELECTION -1

/**
 * Allows an owning vote plugin to replace NativeVotes' raw progress counts.
 *
 * clientChoices contains only selections accepted by NativeVotes. itemVotes
 * starts with raw item counts and must remain indexed by the vote item index.
 * Return Plugin_Changed after replacing itemVotes and weightedVoteTotal.
 */
forward Action NativeVotes_OnBuildProgress(
	NativeVote vote,
	const int[] clientChoices,
	int clientChoiceCount,
	int[] itemVotes,
	int itemCount,
	int &weightedVoteTotal);
