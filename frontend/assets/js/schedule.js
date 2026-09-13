let allMatches = [];

let currentRound = 1;


/* =========================================================
   INITIAL LOAD
   ========================================================= */

document.addEventListener(
    'DOMContentLoaded',
    () => {

        loadSchedule();

    }
);


/* =========================================================
   LOAD SCHEDULE
   ========================================================= */

async function loadSchedule() {

    try {

        if (!tournamentId) {
            return;
        }


        const data = await API.get(
            `/api/tournaments/${tournamentId}/schedule`
        );


        /*
            Validate the response from the C API.
        */

        allMatches = API.records(
            data,
            [
                'id',
                'round',
                'home',
                'away',
                'status'
            ],
            'Schedule'
        );


        /*
            Get all unique rounds.

            Number() is used because the C API may return
            the round value as either a number or a string.

            Numeric sorting prevents:

                Round 1
                Round 10
                Round 2

            and gives us:

                Round 1
                Round 2
                Round 3
                ...
        */

        const rounds = [
            ...new Set(
                allMatches.map(
                    match => Number(match.round)
                )
            )
        ].sort(
            (a, b) => a - b
        );


        /*
            No schedule exists yet.
        */

        if (!rounds.length) {

            $('#tabs').innerHTML = '';

            $('#roundTitle').textContent =
                'No rounds yet';

            $('#matchCount').textContent =
                '0 matches';

            $('#fixtures').innerHTML = `
                <div class="schedule-empty">

                    <div class="schedule-empty-icon">
                        S
                    </div>

                    <h3>
                        No schedule generated
                    </h3>

                    <p>
                        Add at least two teams and generate
                        the round-robin schedule.
                    </p>

                </div>
            `;

            return;
        }


        /*
            If the previously selected round no longer
            exists, select the first available round.
        */

        if (!rounds.includes(currentRound)) {

            currentRound = rounds[0];

        }


        renderRoundTabs(rounds);

        showRound(currentRound);


    } catch (error) {

        toast(
            error.message,
            true
        );

    }

}


/* =========================================================
   RENDER ROUND TABS
   ========================================================= */

function renderRoundTabs(rounds) {

    const tabs = $('#tabs');


    /*
        Create the round buttons.

        data-round is important because it lets us
        identify the exact round without trying to
        read the text of the button.
    */

    tabs.innerHTML = rounds
        .map(
            round => `
                <button
                    type="button"
                    class="round-tab ${
                        Number(round) === Number(currentRound)
                            ? 'active'
                            : ''
                    }"
                    data-round="${round}"
                    role="tab"
                    aria-selected="${
                        Number(round) === Number(currentRound)
                            ? 'true'
                            : 'false'
                    }"
                    onclick="showRound(${round})"
                >

                    <span class="round-tab-number">
                        ${round}
                    </span>

                    <span>
                        Round ${round}
                    </span>

                </button>
            `
        )
        .join('');

}


/* =========================================================
   SHOW ROUND
   ========================================================= */

function showRound(roundNumber) {

    currentRound = Number(roundNumber);


    /*
        Get only the matches belonging to
        the selected round.
    */

    const matches =
        allMatches.filter(
            match =>
                Number(match.round) === currentRound
        );


    /*
        Update heading.
    */

    $('#roundTitle').textContent =
        `Round ${currentRound}`;


    /*
        Update match count.
    */

    $('#matchCount').textContent =
        `${matches.length} ${
            matches.length === 1
                ? 'match'
                : 'matches'
        }`;


    /*
        Update active round button.

        Because every button has data-round,
        there is no need to parse button text.
    */

    document
        .querySelectorAll('.round-tab')
        .forEach(
            button => {

                const buttonRound =
                    Number(
                        button.dataset.round
                    );


                const active =
                    buttonRound === currentRound;


                button.classList.toggle(
                    'active',
                    active
                );


                button.setAttribute(
                    'aria-selected',
                    active
                        ? 'true'
                        : 'false'
                );

            }
        );


    /*
        If there are no matches for this round,
        show an empty state.
    */

    if (!matches.length) {

        $('#fixtures').innerHTML = `
            <div class="schedule-empty">

                <div class="schedule-empty-icon">
                    S
                </div>

                <h3>
                    No matches in this round
                </h3>

                <p>
                    There are no fixtures available
                    for Round ${currentRound}.
                </p>

            </div>
        `;

        return;
    }


    /*
        Render every match in the selected round.
    */

    $('#fixtures').innerHTML =
        matches
            .map(
                (match, index) =>
                    renderMatch(
                        match,
                        index + 1
                    )
            )
            .join('');

}


/* =========================================================
   RENDER MATCH
   ========================================================= */

function renderMatch(
    match,
    matchNumber
) {

    /*
        Check whether this is a BYE.

        A BYE means a team does not have an opponent
        in that round.
    */

    const isBye =
        String(match.status)
            .toUpperCase() === 'BYE';


    /*
        Get match status.

        If the API doesn't provide one,
        use UPCOMING.
    */

    const status =
        String(
            match.status || 'UPCOMING'
        );


    /*
        Convert status into a CSS class.

        Examples:

            Completed -> completed
            Upcoming  -> upcoming
            BYE       -> bye
    */

    const statusClass =
        status
            .toLowerCase()
            .replace(
                /\s+/g,
                '-'
            );


    return `
        <article
            class="schedule-match-card"
        >

            <!-- MATCH NUMBER -->

            <div class="match-number">

                <span>
                    MATCH
                </span>

                <strong>
                    ${matchNumber}
                </strong>

            </div>


            <!-- TEAMS -->

            <div class="match-teams">


                <!-- HOME TEAM -->

                <div class="schedule-team home-team">

                    <span class="team-name">
                        ${escapeHtml(match.home)}
                    </span>

                    ${
                        !isBye
                            ? '<span class="team-side">HOME</span>'
                            : ''
                    }

                </div>


                <!-- VS -->

                <div class="match-vs">

                    ${
                        isBye
                            ? '<span class="bye-symbol">—</span>'
                            : '<span>VS</span>'
                    }

                </div>


                <!-- AWAY TEAM -->

                <div class="schedule-team away-team">

                    <span class="team-name">
                        ${escapeHtml(match.away)}
                    </span>

                    ${
                        !isBye
                            ? '<span class="team-side">AWAY</span>'
                            : ''
                    }

                </div>


            </div>


            <!-- STATUS -->

            <div class="match-status">

                <span
                    class="schedule-status ${statusClass}"
                >
                    ${escapeHtml(status)}
                </span>

            </div>


        </article>
    `;

}


/* =========================================================
   GENERATE / REGENERATE SCHEDULE
   ========================================================= */

$('#generate').onclick = async () => {


    /*
        Confirm before regenerating because existing
        match results can be replaced.
    */

    const confirmed =
        confirm(
            'Regenerate schedule? Any entered match results will be lost.'
        );


    if (!confirmed) {
        return;
    }


    try {

        await API.post(
            `/api/tournaments/${tournamentId}/schedule/generate`
        );


        toast(
            'Round-robin schedule generated'
        );


        /*
            Start again from Round 1 after generation.
        */

        currentRound = 1;


        /*
            Reload the schedule from the C backend.
        */

        await loadSchedule();


    } catch (error) {

        toast(
            error.message,
            true
        );

    }

};


/* =========================================================
   HTML ESCAPING
   ========================================================= */

function escapeHtml(value) {

    return String(
        value ?? ''
    )
        .replace(
            /&/g,
            '&amp;'
        )
        .replace(
            /</g,
            '&lt;'
        )
        .replace(
            />/g,
            '&gt;'
        )
        .replace(
            /"/g,
            '&quot;'
        )
        .replace(
            /'/g,
            '&#039;'
        );

}
