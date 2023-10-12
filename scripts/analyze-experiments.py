import argparse as ap
import pathlib
import matplotlib as mpl
import matplotlib.pyplot as plt
import pandas as pd
import seaborn as sns
import yaml


def load_single_result(result_file: pathlib.Path) -> pd.DataFrame:
    dataframe = pd.read_json(result_file)["stats"]
    relative_timestamps = [entry["relative_timestamp"] for entry in dataframe]
    coverage = [entry["coverage"] for entry in dataframe]
    crashes = [entry["crashes"] for entry in dataframe]
    execs_sec = [entry["execs_sec"] for entry in dataframe]
    corpus_size = [entry["corpus_size"] for entry in dataframe]
    dataframe = pd.DataFrame(
        {
            f"relative_timestamp-{result_file.stem}": relative_timestamps,
            f"coverage-{result_file.stem}": coverage,
            f"crashes-{result_file.stem}": crashes,
            f"execs_sec-{result_file.stem}": execs_sec,
            f"corpus_size-{result_file.stem}": corpus_size,
        }
    )
    return dataframe


def load_results(results_dir: pathlib.Path) -> pd.DataFrame:
    dataframe = pd.DataFrame()
    for result in results_dir.glob("*.json"):
        result_df = load_single_result(result)
        dataframe = pd.concat([dataframe, result_df], axis=1)

    return dataframe


def load_experiment_config(exp_config: pathlib.Path) -> tuple:
    with open(exp_config, "r", encoding="ascii") as conf_stream:
        try:
            config = yaml.safe_load(conf_stream)
        except yaml.YAMLError as exc:
            print(exc)
            exit(1)

    results_dir = pathlib.Path(config.get("results-dir", "experiment-results")).resolve()
    base_config = pathlib.Path(config.get("base-config", None)).stem
    alternative_config = pathlib.Path(config.get("alternative-config", None)).stem

    return (results_dir, base_config, alternative_config)


def plot_results(results: pd.DataFrame, base: str, alternative: str):
    sns.set_theme(style="darkgrid")

    IGNORE_FIRST_N = 10
    IGNORE_LAST_N = -22

    for graph_type in ("coverage", "execs_sec", "corpus_size"):
        base_mean = results.iloc[
            IGNORE_FIRST_N:IGNORE_LAST_N,
            results.columns.str.contains(f"{graph_type}-bb-coverage-.*-{base}"),
        ].mean(axis=1)
        base_std = results.iloc[
            IGNORE_FIRST_N:IGNORE_LAST_N,
            results.columns.str.contains(f"{graph_type}-bb-coverage-.*-{base}"),
        ].std(axis=1)

        alt_mean = results.iloc[
            IGNORE_FIRST_N:IGNORE_LAST_N,
            results.columns.str.contains(f"{graph_type}-bb-coverage-.*-{alternative}"),
        ].mean(axis=1)
        alt_std = results.iloc[
            IGNORE_FIRST_N:IGNORE_LAST_N,
            results.columns.str.contains(f"{graph_type}-bb-coverage-.*-{alternative}"),
        ].std(axis=1)

        start_time = results.iloc[
            IGNORE_FIRST_N,
            results.columns.str.contains("relative_timestamp-bb-coverage-.*-.*"),
        ].mean()
        end_time = results.iloc[
            IGNORE_LAST_N,
            results.columns.str.contains("relative_timestamp-bb-coverage-.*-.*"),
        ].mean()

        plt.figure(dpi=150)
        ax = plt.subplot()
        ax.xaxis.set_major_formatter(mpl.dates.DateFormatter("%H:%M"))
        ax.xaxis.set_major_locator(mpl.dates.MinuteLocator(interval=30))
        ax.set_xlabel("time")
        ax.set_xlim(
            pd.to_datetime(start_time - 400, unit="s"), pd.to_datetime(end_time + 400, unit="s")
        )
        plt.xticks(rotation=45)

        timestamps = pd.date_range(
            start=pd.to_datetime(start_time, unit="s"),
            end=pd.to_datetime(end_time, unit="s"),
            periods=len(base_mean),
        )

        ax.plot(
            timestamps,
            base_mean,
            label="no-laf",
            linewidth=2,
        )
        ax.plot(
            timestamps,
            alt_mean,
            label="laf",
            linestyle="--",
            linewidth=2,
        )
        ax.legend()
        ax.set(title=graph_type)

        ax.fill_between(
            timestamps,
            base_mean - base_std,
            base_mean + base_std,
            alpha=0.2,
        )
        ax.fill_between(
            timestamps,
            alt_mean - alt_std,
            alt_mean + alt_std,
            alpha=0.2,
        )

    COV_ENTRY_N = results.index[-30]

    plt.figure()
    sns.boxplot(
        data=pd.DataFrame(
            {
                "no-laf": results.loc[
                    COV_ENTRY_N,
                    results.columns.str.contains(f"coverage-bb-coverage-.*-{base}"),
                ],
                "laf": results.loc[
                    COV_ENTRY_N,
                    results.columns.str.contains(f"coverage-bb-coverage-.*-{alternative}"),
                ],
            }
        ),
    )

    plt.show()


def main():
    parser = ap.ArgumentParser()
    parser.add_argument(
        "-b",
        "--base",
        type=str,
        help="Name of the base configuration (e.g. no-laf)",
    )
    parser.add_argument(
        "-a",
        "--alternative",
        type=str,
        help="Name of the alternative configuration (e.g. laf)",
    )
    parser.add_argument(
        "results",
        type=pathlib.Path,
        help="Path to the results directory",
    )
    args = parser.parse_args()

    base_name = args.base
    alternative_name = args.alternative
    results_dir = args.results

    results = load_results(results_dir, base_name, alternative_name)
    plot_results(results, base_name, alternative_name)


if __name__ == "__main__":
    main()
