"""Probability Lab - theory vs simulation.

One Streamlit tab per lecture module.  Every tab follows the same shape:

    exact formula  ->  the numbers that come out of it  ->  a NumPy simulation
    ->  the simulated estimate next to the exact one, with the difference
        measured in standard errors.

Run it with ``nmake run`` (or ``streamlit run app.py``).
"""

from __future__ import annotations

from typing import Sequence

import matplotlib.pyplot as plt
import numpy as np
import streamlit as st

from src import bayes, continuous, counting, discrete, joint, moments, simulate

st.set_page_config(page_title="Probability Lab", layout="wide")


# ----------------------------------------------------------------- app plumbing
def comparison_bars(
    categories: Sequence,
    exact: Sequence[float],
    simulated: Sequence[float],
    xlabel: str = "",
    ylabel: str = "probability",
    title: str = "",
):
    """Exact values next to simulated frequencies, the figure every tab needs."""
    figure, axes = plt.subplots(figsize=(9, 3.2))
    positions = np.arange(len(categories))
    axes.bar(positions - 0.2, exact, width=0.4, label="exact")
    axes.bar(positions + 0.2, simulated, width=0.4, label="simulated")
    axes.set_xticks(positions)
    axes.set_xticklabels([str(c) for c in categories])
    axes.set_xlabel(xlabel)
    axes.set_ylabel(ylabel)
    if title:
        axes.set_title(title)
    axes.legend(frameon=False)
    figure.tight_layout()
    return figure


def show(figure) -> None:
    """Render a matplotlib figure and release it."""
    st.pyplot(figure, clear_figure=True)


def metrics(pairs: Sequence[tuple[str, str]]) -> None:
    """A row of metric cards."""
    for column, (label, value) in zip(st.columns(len(pairs)), pairs):
        column.metric(label, value)


def agreement_caption(comparison: dict) -> None:
    """One line describing how far the simulation is from the exact answer."""
    if comparison["agrees"]:
        st.success(
            f"Agrees: estimate {comparison['estimate']:.5f} vs exact {comparison['exact']:.5f} "
            f"(z = {comparison['z_score']:+.2f}, tolerance {comparison['tolerance']:.5f})."
        )
    else:
        st.warning(
            f"Outside the 2-standard-error band: estimate {comparison['estimate']:.5f} vs exact "
            f"{comparison['exact']:.5f} (z = {comparison['z_score']:+.2f})."
        )


@st.cache_data(show_spinner=False)
def die_sample(n_draws: int, seed: int) -> np.ndarray:
    """One fair die, sampled - the discrete uniform is not in :mod:`src.simulate`."""
    return simulate.rng(seed).integers(1, 7, size=n_draws).astype(float)


st.title("Probability Lab - theory vs simulation")
st.caption(
    "Every result is computed from the formula first, then estimated by NumPy simulation, "
    "and the two are compared in units of the estimate's own standard error."
)

tab_counting, tab_bayes, tab_discrete, tab_continuous, tab_moments, tab_joint = st.tabs(
    [
        "W1 - Counting & events",
        "W2 - Bayes' rule",
        "W3 - Discrete distributions",
        "W4 - Continuous distributions",
        "W5 - Expectation & variance",
        "W8 - Joint distributions",
    ]
)


# =============================================================== week 1: counting
with tab_counting:
    st.subheader("Counting & events")
    st.markdown(
        "Three dice give a sample space small enough to enumerate, so the exact "
        "probability is a count. The running example is the one from the dice exercise."
    )
    st.latex(r"P(A) = \frac{|A|}{|\Omega|}, \qquad |\Omega| = 6^3 = 216")

    exact_ge_17 = counting.sum_at_least(17, 3)
    favourable = counting.count_where(lambda outcome: counting.dice_sum(outcome) >= 17, 3)
    metrics(
        [
            ("Outcomes in the sample space", f"{counting.total_outcomes(3):,}"),
            ("Sums of 17 or more", f"{favourable}"),
            ("Exact P(sum >= 17)", f"{exact_ge_17} = {float(exact_ge_17):.4%}"),
        ]
    )

    n_rolls = st.slider("Simulated rolls", 10_000, 200_000, 50_000, step=10_000)
    dice_check = counting.simulate_sum_at_least(17, n_rolls, 3, seed=simulate.DEFAULT_SEED)
    metrics(
        [
            ("Rolls with sum >= 17", f"{dice_check['hits']:,} of {n_rolls:,}"),
            ("Simulated estimate", f"{dice_check['estimate']:.5f}"),
            ("Absolute error", f"{dice_check['abs_error']:.5f}"),
            ("z-score of the error", f"{dice_check['z_score']:+.2f}"),
        ]
    )
    agreement_caption(dice_check)

    rows = counting.sum_distribution_table(3, n_rolls, seed=simulate.DEFAULT_SEED)
    show(
        comparison_bars(
            [row["sum"] for row in rows],
            [row["exact"] for row in rows],
            [row["simulated"] for row in rows],
            xlabel="sum of three dice",
            title="PMF of the sum: counted exactly vs 50 000 simulated rolls",
        )
    )
    st.dataframe(rows, hide_index=True)

    st.divider()
    st.markdown("**Inclusion-exclusion and the complement trick.**")
    st.latex(
        r"P(A \cup B \cup C) = \sum P(A_i) - \sum P(A_i \cap A_j) + P(A \cap B \cap C) "
        r"= \frac{3}{6} - \frac{3}{36} + \frac{1}{216} = \frac{91}{216}"
    )
    six_union = counting.at_least_one_six(3)
    metrics(
        [
            ("Union of the three sixes", f"{six_union:.5f}"),
            ("Complement 1 - (5/6)^3", f"{counting.at_least_one(1 / 6, 3):.5f}"),
            ("Exact fraction 91/216", f"{six_union:.6f}"),
        ]
    )
    six_rows = counting.at_least_one_six_table((1, 2, 3, 4, 5, 6), 50_000, seed=simulate.DEFAULT_SEED)
    show(
        comparison_bars(
            [f"n={row['n']}" for row in six_rows],
            [row["exact"] for row in six_rows],
            [row["simulated"] for row in six_rows],
            xlabel="dice per experiment",
            title="P(at least one six) = 1 - (5/6)^n",
        )
    )
    st.dataframe(six_rows, hide_index=True)

    st.divider()
    st.markdown("**Combinations, permutations, and a counting classic.**")
    metrics(
        [
            ("C(10, 3)", f"{counting.combinations_count(10, 3)}"),
            ("P(10, 3)", f"{counting.permutations_count(10, 3)}"),
            ("P(shared birthday, 23 people)", f"{counting.birthday_collision_probability(23):.4f}"),
        ]
    )
    st.caption(
        "The birthday figure comes from counting ordered outcomes: "
        "1 - P(365, n) / 365^n, with no simulation involved."
    )


# ================================================================== week 2: Bayes
with tab_bayes:
    st.subheader("Bayes' rule: medical screening, fraud and a spam filter")
    st.latex(
        r"P(D \mid +) = \frac{P(+ \mid D) P(D)}{P(+ \mid D) P(D) + P(+ \mid D^c) P(D^c)}"
    )

    left, middle, right = st.columns(3)
    prevalence = left.slider("Prevalence P(D)", 0.001, 0.20, 0.01, step=0.001, format="%.3f")
    sensitivity = middle.slider("Sensitivity P(+ | D)", 0.50, 0.999, 0.99, step=0.001, format="%.3f")
    specificity = right.slider("Specificity P(- | healthy)", 0.50, 0.999, 0.95, step=0.001, format="%.3f")

    population = st.select_slider("People simulated", options=[1_000, 10_000, 50_000, 100_000], value=10_000)
    screening = bayes.simulate_screening(
        prevalence, sensitivity, specificity, population, seed=simulate.DEFAULT_SEED
    )
    breakdown = bayes.posterior_breakdown(prevalence, sensitivity, specificity)
    frequencies = bayes.natural_frequencies(prevalence, sensitivity, specificity, population)

    metrics(
        [
            ("Exact P(D | +)", f"{breakdown['posterior']:.4%}"),
            ("Simulated P(D | +)", f"{screening['posterior_estimate']:.4%}"),
            ("Positive tests", f"{screening['test_positives']:,}"),
            ("True positives", f"{screening['true_positives']:,}"),
            ("False positives", f"{screening['false_positives']:,}"),
        ]
    )
    agreement_caption(screening["comparison"])

    st.markdown(
        f"**Natural frequencies, out of {population:,} people** - the lecture's version of the "
        "same arithmetic: "
        f"{frequencies['diseased']:.0f} diseased, of whom {frequencies['true_positives']:.0f} test positive; "
        f"{frequencies['healthy']:.0f} healthy, of whom {frequencies['false_positives']:.0f} test positive."
    )
    st.dataframe(
        [
            {"group": "people", "simulated": screening["population"], "exact": population},
            {"group": "diseased", "simulated": screening["diseased"], "exact": round(frequencies["diseased"])},
            {"group": "positive tests", "simulated": screening["test_positives"], "exact": round(frequencies["test_positives"])},
            {"group": "true positives", "simulated": screening["true_positives"], "exact": round(frequencies["true_positives"])},
            {"group": "false positives", "simulated": screening["false_positives"], "exact": round(frequencies["false_positives"])},
        ],
        hide_index=True,
    )

    prevalences = [0.001, 0.002, 0.005, 0.01, 0.02, 0.05, 0.1, 0.2]
    figure, axes = plt.subplots(figsize=(9, 3.0))
    axes.plot(prevalences, [bayes.posterior(p, sensitivity, specificity) for p in prevalences], marker="o")
    axes.set_xscale("log")
    axes.set_xlabel("prevalence P(D) (log scale)")
    axes.set_ylabel("P(D | +)")
    axes.set_title("Why a rare disease makes a positive test unimpressive")
    axes.grid(alpha=0.3)
    figure.tight_layout()
    show(figure)

    st.divider()
    st.markdown("**Two positive tests (midterm review).** Each new positive is fed back in as the prior.")
    two = bayes.two_positive_tests(prevalence, sensitivity, specificity)
    metrics(
        [
            ("After one positive", f"{two['after_first']:.4%}"),
            ("After two (sequential)", f"{two['after_second_sequential']:.4%}"),
            ("After two (P(++ | D) = sens^2)", f"{two['after_second_direct']:.4%}"),
            ("P(+,+ | healthy)", f"{two['positive_twice_given_healthy']:.6f}"),
        ]
    )
    st.dataframe(bayes.sequential_update(prevalence, sensitivity, specificity, 3), hide_index=True)
    twice = bayes.simulate_two_tests(
        prevalence, sensitivity, specificity, 100_000, seed=simulate.DEFAULT_SEED
    )
    st.caption(
        f"Simulated: {twice['true_positives']:,} true vs {twice['false_positives']:,} false positives "
        f"among {twice['positive_twice']:,} doubly positive people -> "
        f"{twice['posterior_estimate']:.4%}."
    )

    st.divider()
    st.markdown("**Fraud detection** - the same arithmetic with a different story.")
    fraud = bayes.fraud_detection(seed=simulate.DEFAULT_SEED)
    metrics(
        [
            ("P(fraud) before the flag", f"{fraud['prior_fraud']:.2%}"),
            ("P(flag | fraud)", f"{fraud['detection_rate']:.0%}"),
            ("P(flag | legitimate)", f"{fraud['false_alarm_rate']:.0%}"),
            ("P(fraud | flag)", f"{fraud['posterior']:.2%}"),
            ("Simulated", f"{fraud['simulation']['posterior_estimate']:.2%}"),
        ]
    )
    st.dataframe(
        [
            {"quantity": "flagged transactions", "exact": f"{fraud['frequencies']['test_positives']:.0f}",
             "simulated": fraud["simulation"]["test_positives"]},
            {"quantity": "of which fraudulent", "exact": f"{fraud['frequencies']['true_positives']:.0f}",
             "simulated": fraud["simulation"]["true_positives"]},
            {"quantity": "of which legitimate", "exact": f"{fraud['frequencies']['false_positives']:.0f}",
             "simulated": fraud["simulation"]["false_positives"]},
        ],
        hide_index=True,
    )

    st.divider()
    st.markdown("**Naive-Bayes spam filter.** Word present/absent evidence, scored in log space.")
    present_words = st.multiselect(
        "Words in the message",
        options=sorted(bayes.SPAM_WORD_GIVEN_SPAM),
        default=["free", "winner", "click"],
    )
    spam = bayes.naive_bayes_spam(
        bayes.SPAM_WORD_GIVEN_SPAM, bayes.SPAM_WORD_GIVEN_HAM, present_words, bayes.SPAM_PRIOR
    )
    metrics(
        [
            ("P(spam) prior", f"{spam['prior_spam']:.0%}"),
            ("P(message | spam) term", f"{spam['spam_score']:.3e}"),
            ("P(message | ham) term", f"{spam['ham_score']:.3e}"),
            ("P(spam | message)", f"{spam['posterior_spam']:.4%}"),
        ]
    )
    spam_simulation = bayes.simulate_spam_filter(
        bayes.SPAM_WORD_GIVEN_SPAM,
        bayes.SPAM_WORD_GIVEN_HAM,
        present_words,
        bayes.SPAM_PRIOR,
        seed=simulate.DEFAULT_SEED,
    )
    st.caption(
        f"Simulated over {spam_simulation['n_messages']:,} generated messages: "
        f"{spam_simulation['spam_matches']:,} spam out of {spam_simulation['matches']:,} with this exact "
        f"word pattern -> {spam_simulation['posterior_estimate']:.4%}."
    )
    st.dataframe(spam["contributions"], hide_index=True)


# ============================================================== week 3: discrete
with tab_discrete:
    st.subheader("Binomial, Geometric and Poisson - written from scratch")
    st.markdown(
        "The PMFs below are the lecture formulas, the CDFs are explicit sums of them, and the "
        "binomial coefficient is evaluated in log space so that n = 1000 is no harder than n = 10."
    )

    n_trials = st.slider("Binomial n", 2, 50, 10)
    p_success = st.slider("Binomial p", 0.01, 0.99, 0.30, step=0.01)
    binomial_rows = discrete.binomial_table(n_trials, p_success)
    binomial_check = discrete.simulate_binomial_check(
        n_trials, p_success, 50_000, seed=simulate.DEFAULT_SEED
    )
    metrics(
        [
            ("Exact mean np", f"{discrete.binomial_mean(n_trials, p_success):.4f}"),
            ("Simulated mean", f"{binomial_check['sample_mean']:.4f}"),
            ("Exact variance np(1-p)", f"{discrete.binomial_variance(n_trials, p_success):.4f}"),
            ("Simulated variance", f"{binomial_check['sample_variance']:.4f}"),
        ]
    )
    show(
        comparison_bars(
            [row["k"] for row in binomial_check["rows"]],
            [row["exact"] for row in binomial_check["rows"]],
            [row["simulated"] for row in binomial_check["rows"]],
            xlabel="k successes",
            title=f"Binomial({n_trials}, {p_success}) PMF vs 50 000 simulated experiments",
        )
    )
    st.dataframe(binomial_rows, hide_index=True)

    st.divider()
    st.markdown("**Geometric: memoryless, and only the geometric is.**")
    p_geom = st.slider("Geometric p", 0.05, 0.9, 0.20, step=0.05)
    geometric_check = discrete.simulate_geometric_check(p_geom, 50_000, seed=simulate.DEFAULT_SEED)
    memoryless = discrete.memoryless_check(p_geom, s=3, t=2)
    counterexample = discrete.memoryless_counterexample(s=2, t=2)
    metrics(
        [
            ("P(X > s+t | X > s)", f"{memoryless['conditional']:.5f}"),
            ("P(X > t)", f"{memoryless['unconditional']:.5f}"),
            ("Difference", f"{memoryless['difference']:.2e}"),
            ("Exact mean / variance", f"{discrete.geometric_mean(p_geom):.3f} / {discrete.geometric_variance(p_geom):.3f}"),
            ("Simulated mean / variance", f"{geometric_check['sample_mean']:.3f} / {geometric_check['sample_variance']:.3f}"),
        ]
    )
    st.dataframe(
        [
            {
                "distribution": memoryless["distribution"],
                "s": memoryless["s"],
                "t": memoryless["t"],
                "P(X > s+t | X > s)": memoryless["conditional"],
                "P(X > t)": memoryless["unconditional"],
                "memoryless": memoryless["memoryless"],
            },
            {
                "distribution": counterexample["distribution"],
                "s": counterexample["s"],
                "t": counterexample["t"],
                "P(X > s+t | X > s)": counterexample["conditional"],
                "P(X > t)": counterexample["unconditional"],
                "memoryless": counterexample["memoryless"],
            },
        ],
        hide_index=True,
    )
    show(
        comparison_bars(
            [row["k"] for row in geometric_check["rows"]],
            [row["exact"] for row in geometric_check["rows"]],
            [row["simulated"] for row in geometric_check["rows"]],
            xlabel="trials until the first success",
            title=f"Geometric({p_geom}) PMF vs simulation",
        )
    )
    st.dataframe(discrete.geometric_table(p_geom, 12), hide_index=True)

    st.divider()
    st.markdown("**Poisson.**")
    lam = st.slider("Poisson lambda", 0.5, 12.0, 3.0, step=0.5)
    poisson_check = discrete.simulate_poisson_check(lam, 50_000, seed=simulate.DEFAULT_SEED)
    metrics(
        [
            ("Exact mean = variance", f"{discrete.poisson_mean(lam):.3f}"),
            ("Simulated mean", f"{poisson_check['sample_mean']:.3f}"),
            ("Simulated variance", f"{poisson_check['sample_variance']:.3f}"),
            ("z-score of the mean", f"{poisson_check['mean_comparison']['z_score']:+.2f}"),
        ]
    )
    show(
        comparison_bars(
            [row["k"] for row in poisson_check["rows"]],
            [row["exact"] for row in poisson_check["rows"]],
            [row["simulated"] for row in poisson_check["rows"]],
            xlabel="counts k",
            title=f"Poisson({lam}) PMF vs simulation",
        )
    )
    st.dataframe(discrete.poisson_table(lam, 12), hide_index=True)

    st.divider()
    st.markdown("**The Poisson approximation to the Binomial as n grows.** Fixing lam = np switches components.")
    approximation = discrete.poisson_approximation((5, 10, 20, 50, 100, 500, 1000), 1.0)
    st.dataframe(
        [
            {
                "n": row["n"],
                "p = lam / n": row["p"],
                "max |binomial - poisson|": row["max_abs_error"],
                "total variation distance": row["total_variation"],
            }
            for row in approximation
        ],
        hide_index=True,
    )
    show(
        comparison_bars(
            approximation[-1]["k"],
            approximation[-1]["binomial"],
            approximation[-1]["poisson"],
            xlabel="defects k",
            ylabel="probability",
            title="Binomial(1000, 0.001) vs Poisson(1) - the 1 000-component example",
        )
    )
    st.dataframe(discrete.defect_example(), hide_index=True)


# ============================================================ week 4: continuous
with tab_continuous:
    st.subheader("Continuous distributions")
    st.latex(r"\int_{-\infty}^{\infty} f(x)\,dx = 1, \qquad F(x) = \int_{-\infty}^{x} f(t)\,dt")

    st.markdown("**Does each density integrate to 1?** Trapezium and Simpson, on a truncated support.")
    st.dataframe(continuous.integrates_to_one_table(), hide_index=True)

    st.divider()
    st.markdown("**Normal: percentiles by standardising.**")
    mu = st.slider("mu", -3.0, 3.0, 0.0, step=0.5)
    sigma = st.slider("sigma", 0.5, 3.0, 1.0, step=0.5)
    percentile_rows = continuous.normal_percentiles(mu, sigma, (0.5, 0.9, 0.95, 0.975, 0.99))
    st.dataframe(percentile_rows, hide_index=True)
    st.caption("The `standardised` column re-applies (x - mu)/sigma, so the round trip has to close.")

    grid = np.linspace(mu - 4 * sigma, mu + 4 * sigma, 400)
    figure, axes = plt.subplots(1, 2, figsize=(11, 3.2))
    axes[0].plot(grid, continuous.grid_values(lambda x: continuous.normal_pdf(x, mu, sigma), grid))
    axes[0].set_title(f"Normal({mu}, {sigma}^2) density")
    axes[1].plot(grid, continuous.grid_values(lambda x: continuous.normal_cdf(x, mu, sigma), grid), label="analytic")
    comparison_rows = continuous.cdf_comparison(
        lambda x: continuous.normal_pdf(x, mu, sigma),
        lambda x: continuous.normal_cdf(x, mu, sigma),
        mu - 4 * sigma,
        np.linspace(mu - 3 * sigma, mu + 3 * sigma, 15),
    )
    axes[1].plot(
        [row["x"] for row in comparison_rows],
        [row["numeric"] for row in comparison_rows],
        "o",
        markersize=4,
        label="integrated PDF",
    )
    axes[1].set_title("CDF: formula vs numeric integration")
    axes[1].legend(frameon=False)
    for axis in axes:
        axis.grid(alpha=0.3)
    figure.tight_layout()
    show(figure)
    st.dataframe(comparison_rows, hide_index=True)

    st.divider()
    st.markdown("**Exponential waiting times, and the Poisson process behind them.**")
    rate = st.slider("Arrival rate (per unit time)", 0.2, 5.0, 1.0, step=0.2)
    waiting = continuous.waiting_time_check(rate, 50_000, seed=simulate.DEFAULT_SEED)
    metrics(
        [
            ("Exact mean 1/rate", f"{waiting['theoretical_mean']:.4f}"),
            ("Simulated mean", f"{waiting['sample_mean']:.4f}"),
            ("Exact variance", f"{waiting['theoretical_variance']:.4f}"),
            ("Simulated variance", f"{waiting['sample_variance']:.4f}"),
        ]
    )
    figure, axes = plt.subplots(figsize=(9, 3.0))
    axes.hist(waiting["samples"], bins=60, density=True, alpha=0.6, label="simulated waiting times")
    grid = np.linspace(0.0, 5.0 / rate, 300)
    axes.plot(grid, continuous.grid_values(lambda x: continuous.exponential_pdf(x, 1.0 / rate), grid), label="Exp(1/rate) density")
    axes.set_xlabel("waiting time")
    axes.set_ylabel("density")
    axes.legend(frameon=False)
    axes.grid(alpha=0.3)
    figure.tight_layout()
    show(figure)

    horizon = st.slider("Horizon", 20.0, 400.0, 200.0, step=20.0)
    bin_width = st.slider("Interval width", 0.5, 5.0, 1.0, step=0.5)
    process = continuous.simulate_poisson_process(rate, horizon, bin_width, seed=simulate.DEFAULT_SEED)
    metrics(
        [
            ("Arrivals observed", f"{process['n_arrivals']:,}"),
            ("Expected rate x horizon", f"{process['expected_arrivals']:,.1f}"),
            ("Intervals", f"{process['n_intervals']:,}"),
            ("Mean counts per interval", f"{process['mean_counts']:.3f}"),
            ("Poisson mean lam = rate x width", f"{process['lambda_per_interval']:.2f}"),
        ]
    )
    agreement_caption(process["counts_comparison"])
    show(
        comparison_bars(
            [row["count"] for row in process["table"]],
            [row["poisson"] for row in process["table"]],
            [row["observed"] for row in process["table"]],
            xlabel="arrivals in one interval",
            title="Counts per interval vs the Poisson(lam) PMF",
        )
    )
    st.dataframe(process["table"], hide_index=True)


# ======================================================= week 5: expectation & variance
with tab_moments:
    st.subheader("Expectation and variance, four ways")
    st.latex(
        r"E[X] = \sum_x x\,p(x), \qquad "
        r"\operatorname{Var}(X) = E[(X - E[X])^2] = E[X^2] - (E[X])^2"
    )

    n_draws = st.slider("Simulated draws (one fair die)", 5_000, 100_000, 50_000, step=5_000)
    values, probs = moments.die_distribution()
    die_report = moments.moment_report(values, probs, die_sample(n_draws, simulate.DEFAULT_SEED))
    metrics(
        [
            ("Exact E[X]", f"{die_report['exact_mean']:.4f}"),
            ("Exact Var(X)", f"{die_report['exact_variance']:.4f}"),
            ("Simulated E[X]", f"{die_report['sample_mean']:.4f}"),
            ("Simulated Var(X)", f"{die_report['sample_variance']:.4f}"),
        ]
    )
    st.dataframe(moments.moment_report_table(die_report), hide_index=True)
    for key in ("mean_comparison", "variance_comparison"):
        comparison = die_report[key]
        st.caption(
            f"{key.split('_')[0]}: estimate {comparison['estimate']:.4f} vs exact "
            f"{comparison['exact']:.4f}, z = {comparison['z_score']:+.2f}, "
            f"{'inside' if comparison['agrees'] else 'outside'} the 2-standard-error band."
        )

    st.divider()
    st.markdown("**Why a Binomial mean is np: linearity with indicators.**")
    st.latex(r"X = \sum_{i=1}^{n} I_i, \qquad E[I_i] = p, \qquad E[X] = \sum_{i=1}^{n} E[I_i] = np")
    st.caption("No independence is needed for linearity - only for the variance to add up as well.")

    n_ind = st.slider("Trials n (indicators)", 2, 40, 10)
    p_ind = st.slider("Success probability p", 0.05, 0.95, 0.30, step=0.05)
    decomposition = moments.indicator_decomposition(n_ind, p_ind)
    linearity = moments.simulate_linearity(n_ind, p_ind, 50_000, seed=simulate.DEFAULT_SEED)
    metrics(
        [
            ("Indicator mean E[I_i]", f"{decomposition['each_indicator_mean']:.2f}"),
            ("Sum of indicator means", f"{decomposition['sum_of_indicator_means']:.4f}"),
            ("Binomial mean np", f"{decomposition['binomial_mean']:.4f}"),
            ("Simulated mean of the count", f"{linearity['mean_of_counts']:.4f}"),
            ("Simulated average indicator", f"{linearity['average_indicator_mean']:.4f}"),
        ]
    )
    figure, axes = plt.subplots(1, 2, figsize=(11, 3.2))
    axes[0].bar(range(1, n_ind + 1), linearity["indicator_means"])
    axes[0].axhline(p_ind, color="black", linestyle="--", linewidth=1, label=f"p = {p_ind}")
    axes[0].set_xlabel("indicator i")
    axes[0].set_ylabel("simulated E[I_i]")
    axes[0].set_title("Every indicator has mean p")
    axes[0].legend(frameon=False)
    axes[1].plot(simulate.running_mean(linearity["counts"]), linewidth=1)
    axes[1].axhline(linearity["exact_mean"], color="black", linestyle="--", linewidth=1, label=f"np = {linearity['exact_mean']:.2f}")
    axes[1].set_xlabel("experiments")
    axes[1].set_ylabel("running mean of the count")
    axes[1].set_title("The average of the sum settles on np")
    axes[1].legend(frameon=False)
    for axis in axes:
        axis.grid(alpha=0.3)
    figure.tight_layout()
    show(figure)

    st.divider()
    st.markdown("**Moments from the MGF.** Differentiating e^(tX) inside the sum gives E[X^r] = M^(r)(0).")
    mgf_rows = moments.mgf_moment_table()
    st.dataframe(mgf_rows, hide_index=True)
    worst_truncation = max(max(row["mean_error"], row["variance_error"]) for row in mgf_rows)
    st.caption(
        "The Poisson and Geometric rows use a truncated, renormalised PMF, so their entries are finite "
        f"sums; the truncation error there is below {worst_truncation:.1e}."
    )


# ================================================================== week 8: joint
with tab_joint:
    st.subheader("Joint distributions, and the Y = X^2 trap")
    st.markdown("The joint PMF is counted straight from the sample space, then everything else is read off the table.")

    dice = joint.dice_joint(2)
    st.markdown("**(X, Y) = (first die, sum of both dice)**, counted with itertools.")
    summary = joint.summary(dice["joint"], dice["values_x"], dice["values_y"])
    metrics(
        [
            ("E[X]", f"{summary['E_X']:.4f}"),
            ("E[Y]", f"{summary['E_Y']:.4f}"),
            ("Cov(X, Y)", f"{summary['Cov']:.4f}"),
            ("Corr(X, Y)", f"{summary['Corr']:.4f}"),
            ("Independent?", "yes" if summary["independent"] else "no"),
        ]
    )
    st.caption(
        "Cov(first die, sum) is just Var(first die) = 35/12, because the second die contributes "
        "nothing to the covariance; the correlation is 1 / sqrt(2)."
    )

    figure, axes = plt.subplots(figsize=(8.5, 3.4))
    image = axes.imshow(dice["joint"], cmap="Blues", aspect="auto")
    axes.set_xticks(range(len(dice["values_y"])), dice["values_y"])
    axes.set_yticks(range(len(dice["values_x"])), dice["values_x"])
    axes.set_xlabel("Y = sum")
    axes.set_ylabel("X = first die")
    axes.set_title(f"Joint PMF out of {dice['total']} equally likely outcomes")
    figure.colorbar(image, ax=axes, shrink=0.8)
    figure.tight_layout()
    show(figure)

    st.markdown("**Marginals and conditionals.**")
    st.dataframe(
        [
            {"x": value, "P(X = x)": float(probability)}
            for value, probability in zip(dice["values_x"], joint.marginal_x(dice["joint"]))
        ],
        hide_index=True,
    )
    st.dataframe(joint.conditional_table(dice["joint"], dice["values_x"], dice["values_y"]), hide_index=True)
    st.caption(
        f"Iterated expectation closes the loop: E[E[Y | X]] = {summary['E_conditional_Y_given_X']:.4f} "
        f"= E[Y] = {summary['E_Y']:.4f} (error {summary['iterated_expectation_error']:.2e})."
    )
    simulated_joint = joint.simulate_dice_joint(100_000, 2, seed=simulate.DEFAULT_SEED)
    st.caption(
        f"Simulated over 100 000 rolls: covariance {simulated_joint['covariance_estimate']:.4f} "
        f"(exact {simulated_joint['covariance_exact']:.4f}), correlation "
        f"{simulated_joint['correlation_estimate']:.4f} (exact {simulated_joint['correlation_exact']:.4f}, "
        f"z = {simulated_joint['correlation_comparison']['z_score']:+.2f})."
    )

    st.divider()
    st.markdown("**The trap: Y = X^2 with a symmetric X. Zero correlation, total dependence.**")
    trap = joint.x_squared_trap()
    trap_summary = trap["summary"]
    metrics(
        [
            ("Cov(X, Y)", f"{trap_summary['Cov']:.1e}"),
            ("Corr(X, Y)", f"{trap_summary['Corr']:.1e}"),
            ("Independent?", "yes" if trap_summary["independent"] else "no"),
            ("P(Y = 4 | X = 2)", f"{trap['witness']['P(Y = y | X = x)']:.2f}"),
            ("P(Y = 4)", f"{trap['witness']['P(Y = y)']:.2f}"),
        ]
    )
    st.caption(
        "E[X] = 0 and E[XY] = E[X^3] = 0 by symmetry, so the covariance vanishes - yet knowing X = 2 "
        "pins Y down exactly. Correlation measures linear association, not dependence."
    )
    st.dataframe(
        [
            {
                "x": row["x"],
                "y": row["y"],
                "P(X = x, Y = y)": row["P(X = x, Y = y)"],
            }
            for row in joint.joint_table(trap["joint"], trap["values_x"], trap["values_y"])
        ],
        hide_index=True,
    )
    simulated_trap = joint.simulate_x_squared_trap(100_000, seed=simulate.DEFAULT_SEED)
    st.caption(
        f"Simulated correlation {simulated_trap['correlation_estimate']:+.4f} (zero within "
        f"{1.96 / np.sqrt(100_000):.4f}); simulated P(Y = 4) = {simulated_trap['P_y_at_witness']:.4f} "
        f"versus P(Y = 4 | X = 2) = {simulated_trap['P_y_given_x_at_witness']:.4f}."
    )
    st.dataframe(
        [joint.independence_check(trap["joint"], trap["values_x"], trap["values_y"])],
        hide_index=True,
    )
