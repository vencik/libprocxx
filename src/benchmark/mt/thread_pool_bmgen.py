#!/usr/bin/env python3

import sys
import json
import datetime
import numpy


def _timestamp2datetime(ts):
    secs = int(ts)
    return datetime.datetime.fromtimestamp(secs) \
        + datetime.timedelta(0, ts - secs)


def record2tpoints(rec):
    """
    Converts thread pool info record to graph time points specification.
    Each record will produce the following graph points:
    * Number of running threads
    * Number of jobs in the job queue
    :param rec: Thread pool info record
    :return: Graph time point specification
    """
    def tp_colour(rec):
        if rec["tcnt"] >  rec["tmax"]      : return "red"
        if rec["tcnt"] <  rec["treserved"] : return "orange"
        if rec["tcnt"] == rec["treserved"] : return "green"
        return "blue"

    def jq_colour(rec):
        if rec["queue"]["closed"]                 : return "black"
        #if rec["queue"]["statistics"]["diff"] > 0 : return "red"
        return "green"

    return (
        # Timestamp
        _timestamp2datetime(rec["timestamp"]),
    {
        # Running threads
        "colour"    : tp_colour(rec),
        "value"     : rec["tcnt"],
    }, {
        # Jobs in the queue
        "colour"    : jq_colour(rec),
        "value"     : rec["queue"]["size"],
    })


def main(argv):
    """
    Reads records from input and create benchmark graph.
    :param argv: Command line arguments
    :return: Exit code
    """
    image = argv[1] if len(argv) > 1 else None

    import matplotlib
    if image is not None: matplotlib.use("Agg")
    import matplotlib.pyplot

    vlines = []
    hlines = []

    series = []

    tmax           = None
    jq_closed_last = None
    for line in sys.stdin:
        rec = json.loads(line.rstrip())

        if tmax is None: tmax = rec["tmax"]

        jq_closed = rec["queue"]["closed"]
        if jq_closed_last != jq_closed:
            if jq_closed:
                vlines.append(_timestamp2datetime(rec["timestamp"]))

        jq_closed_last = jq_closed
        series.append(record2tpoints(rec))

    if tmax != None: hlines.append(tmax)

    # TBD: Is this necessary?
    series.sort(key = lambda i: i[0])

    # Produce graphs
    series = list(zip(*map(lambda i: (
        i[0],
        i[1]["value"],
        i[2]["value"]),
    series)))

    matplotlib.pyplot.plot(
        numpy.array(series[0]),
        numpy.array(series[1]),
        zorder=10,
        label="thread count")

    matplotlib.pyplot.plot(
        numpy.array(series[0]),
        numpy.array(series[2]),
        zorder=5,
        label="queue size")

    for x in vlines:
        matplotlib.pyplot.axvline(x, linewidth=1, color="k", zorder=0)

    for y in hlines:
        matplotlib.pyplot.axhline(y, linewidth=1, color="k", zorder=0)

    matplotlib.pyplot.title("Thread pool benchmark")
    matplotlib.pyplot.legend()

    if image is None:
        matplotlib.pyplot.show()
    else:
        fig = matplotlib.pyplot.gcf()
        fig.savefig(image, dpi=100)

    matplotlib.pyplot.clf()

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
