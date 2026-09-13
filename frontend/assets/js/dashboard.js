/* =========================================================
   TOURNAMENT HUB
   Dashboard
========================================================= */


document.addEventListener(
    'DOMContentLoaded',
    async () => {

        try {

            const list =
                await ensureTournament();


            if (!list.length) {

                renderEmptyDashboard();

                return;
            }


            const tournament =
                await API.get(
                    `/api/tournaments/${tournamentId}`
                );


            const matches =
                API.records(

                    await API.get(
                        `/api/tournaments/${tournamentId}/matches`
                    ),

                    [
                        'round',
                        'home',
                        'away',
                        'status'
                    ],

                    'Matches'
                );


            renderDashboard(
                tournament,
                matches
            );

        }
        catch (e) {

            toast(
                e.message,
                true
            );

        }

    }
);



/* =========================================================
   EMPTY DASHBOARD
========================================================= */

function renderEmptyDashboard() {

    $('#title').textContent =
        'Tournament Dashboard';


    $('#content').innerHTML = `

        <section class="hero hero-empty">

            <div class="hero-copy">

                <div class="hero-kicker">
                    Tournament Management
                </div>


                <h2>
                    Build the perfect
                    <span>round robin.</span>
                </h2>


                <p class="hero-description">
                    Create tournaments, manage teams,
                    generate fixtures, record results
                    and track standings from one place.
                </p>


                <div class="hero-actions">

                    <button
                        class="hero-button"
                        id="new"
                    >
                        Create Tournament
                    </button>

                </div>

            </div>

        </section>


        <section
            class="panel workspace-panel"
        >

            <div class="empty">

                <div class="empty-icon">
                    TH
                </div>

                <h2>
                    Your tournament workspace is ready
                </h2>

                <p>
                    Create your first tournament to start
                    adding teams and generating your
                    round-robin schedule.
                </p>


                <button
                    class="btn btn-primary-light"
                    id="newEmpty"
                >
                    Create Tournament
                </button>

            </div>

        </section>

    `;


    $('#new').onclick =
        createTournament;


    $('#newEmpty').onclick =
        createTournament;
}



/* =========================================================
   NORMAL DASHBOARD
========================================================= */

function renderDashboard(
    tournament,
    matches
) {

    $('#title').textContent =
        tournament.name;


    const normalMatches =
        matches.filter(
            match =>
                match.status !== 'BYE'
        );


    const completed =
        normalMatches.filter(
            match =>
                match.status === 'Completed'
        );


    const upcoming =
        normalMatches.filter(
            match =>
                match.status === 'Upcoming'
        );


    $('#content').innerHTML = `


        <!-- =====================================
             HERO
        ====================================== -->

        <section class="hero">


            <div class="hero-copy">


                <div class="hero-kicker">
                    ROUND ROBIN TOURNAMENT
                </div>


                <h2>

                    Every team.

                    <span>
                        Every match.
                    </span>

                    One champion.

                </h2>


                <p class="hero-description">

                    Manage
                    ${escapeHtml(tournament.name)}
                    with automatically generated
                    round-robin fixtures, match results
                    and live standings.

                </p>


                <div class="hero-actions">


                    <a
                        class="hero-button"
                        href="schedule.html"
                    >
                        View Schedule
                    </a>


                    <a
                        class="hero-secondary"
                        href="standings.html"
                    >
                        View Standings
                    </a>


                </div>

            </div>


        </section>



        <!-- =====================================
             STATISTICS
        ====================================== -->

        <section class="dashboard-grid">


            ${statCard(
                'blue',
                'T',
                'Teams',
                tournament.teams
            )}


            ${statCard(
                'green',
                'M',
                'Matches',
                normalMatches.length
            )}


            ${statCard(
                'orange',
                'R',
                'Rounds',
                tournament.rounds
            )}


            ${statCard(
                'purple',
                'C',
                'Completed',
                completed.length
            )}


        </section>



        <!-- =====================================
             MATCH PANELS
        ====================================== -->

        <section class="dashboard-columns">


            <!-- Upcoming -->

            <div class="panel">


                <div class="panel-header">


                    <div class="panel-title">


                        <div class="panel-title-icon">
                            S
                        </div>


                        <div>

                            <h2>
                                Upcoming Matches
                            </h2>

                            <span>
                                Next fixtures in the tournament
                            </span>

                        </div>

                    </div>


                    <a
                        class="view-link"
                        href="schedule.html"
                    >
                        View all
                    </a>


                </div>


                <div class="fixture-list">


                    ${
                        upcoming.length

                        ?

                        upcoming
                            .slice(0, 5)
                            .map(renderFixture)
                            .join('')

                        :

                        `
                        <div class="empty">

                            <div class="empty-icon">
                                S
                            </div>

                            <h2>
                                No upcoming matches
                            </h2>

                            <p>
                                Generate the round-robin
                                schedule to create fixtures.
                            </p>

                        </div>
                        `
                    }


                </div>

            </div>



            <!-- Results -->

            <div class="panel">


                <div class="panel-header">


                    <div class="panel-title">


                        <div class="panel-title-icon">
                            R
                        </div>


                        <div>

                            <h2>
                                Recent Results
                            </h2>

                            <span>
                                Latest completed fixtures
                            </span>

                        </div>

                    </div>


                    <a
                        class="view-link"
                        href="results.html"
                    >
                        View results
                    </a>


                </div>


                <div class="fixture-list">


                    ${
                        completed.length

                        ?

                        completed
                            .slice(-5)
                            .reverse()
                            .map(renderFixture)
                            .join('')

                        :

                        `
                        <div class="empty">

                            <div class="empty-icon">
                                R
                            </div>

                            <h2>
                                No results yet
                            </h2>

                            <p>
                                Completed match results
                                will appear here.
                            </p>

                        </div>
                        `
                    }


                </div>

            </div>


        </section>

    `;
}



/* =========================================================
   STAT CARD
========================================================= */

function statCard(
    type,
    icon,
    label,
    value
) {

    return `

        <div class="stat-card ${type}">


            <div class="stat-top">

                <div class="stat-icon">
                    ${icon}
                </div>

            </div>


            <div class="stat-label">
                ${label}
            </div>


            <div class="stat-value">
                ${value}
            </div>


        </div>

    `;
}



/* =========================================================
   MATCH FIXTURE
========================================================= */

function renderFixture(match) {

    const completed =
        match.status === 'Completed';


    const score =
        completed

        ?

        `
            <span class="fixture-score">

                ${match.homeScore}

                <span class="vs">
                    -
                </span>

                ${match.awayScore}

            </span>
        `

        :

        `
            <span class="fixture-score">
                VS
            </span>
        `;


    return `

        <div class="fixture">


            <div class="fixture-main">


                <div class="fixture-round">
                    R${match.round}
                </div>


                <div class="fixture-teams">


                    <strong>

                        ${escapeHtml(match.home)}

                        <span class="vs">
                            vs
                        </span>

                        ${escapeHtml(match.away)}

                    </strong>


                    <span>
                        Round ${match.round}
                    </span>


                </div>


                ${score}


            </div>


            <span
                class="badge ${match.status}"
            >
                ${match.status}
            </span>


        </div>

    `;
}



/* =========================================================
   CREATE TOURNAMENT
========================================================= */

function createTournament() {

    modal(

        'Create Tournament',

        `

            <div class="field">

                <label>
                    Tournament Name
                </label>


                <input
                    id="name"
                    placeholder="e.g. Intercollege Cup"
                    autocomplete="off"
                >

            </div>


            <div class="field">

                <label>
                    Description
                </label>


                <textarea
                    id="description"
                    placeholder="Describe your tournament..."
                ></textarea>

            </div>

        `,


        async () => {


            const name =
                $('#name')
                    .value
                    .trim();


            const description =
                $('#description')
                    .value
                    .trim();


            if (!name) {

                throw Error(
                    'Tournament name is required.'
                );

            }


            const tournament =
                await API.post(
                    '/api/tournaments',
                    {
                        name,
                        description
                    }
                );


            tournamentId =
                tournament.id;


            localStorage.setItem(
                'tournamentId',
                tournament.id
            );


            location.reload();

        }

    );

}



/* =========================================================
   HTML ESCAPING
========================================================= */

function escapeHtml(value) {

    return String(value ?? '')

        .replaceAll(
            '&',
            '&amp;'
        )

        .replaceAll(
            '<',
            '&lt;'
        )

        .replaceAll(
            '>',
            '&gt;'
        )

        .replaceAll(
            '"',
            '&quot;'
        )

        .replaceAll(
            "'",
            '&#039;'
        );
}
