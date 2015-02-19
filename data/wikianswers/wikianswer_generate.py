import subprocess
import sys

from os import listdir
from os.path import isdir, isfile, join


TARBALL1 = 'http://www.ark.cs.cmu.edu/QA-data/data/Question_Answer_Dataset_v1.2.tar.gz'
TARBALL2 = 'http://www.ark.cs.cmu.edu/QA-data/data/Question_Answer_Dataset_v1.1.tar.gz'

TARBALL = TARBALL1

OUTPUT_FILE = 'wikianswer_dataset.txt'

DATASET_DIR = 'Question_Answer_Dataset'
DATASET_FILE = 'question_answer_pairs.txt'

# Who does that?
WINDOWS_ENCODING = 'latin-1'

def cleanup():
    execute_bash('rm -rf wikianswer.tar.gz %s*' % (DATASET_DIR,))


def execute_bash(command):
    """Executes bash command, prints output and throws an exception on failure."""
    #print(subprocess.check_output(command.split(' '), shell=True))
    process = subprocess.Popen(command,
                               shell=True,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT,
                               universal_newlines=True)
    for line in process.stdout:
        print(line, end='')
    process.wait()
    assert process.returncode == 0


if __name__ == '__main__':
    cleanup()

    execute_bash('wget -O wikianswer.tar.gz %s' % (TARBALL,))
    execute_bash('tar -xz -f wikianswer.tar.gz')
    execute_bash('mv Question_Answer_Dataset* %s' % (DATASET_DIR,))

    directories = [d for d in listdir(DATASET_DIR)
                   if isdir(join(DATASET_DIR, d)) and d.startswith('S')]

    assert len(directories) > 0

    output_content = []
    for d in [join(DATASET_DIR, d) for d in directories]:
        dataset_file = join(d, DATASET_FILE)
        assert isfile(dataset_file)
        with open(dataset_file, newline='', encoding=WINDOWS_ENCODING) as f:
            first = True
            for line in f:
                if first:
                    first = False
                    continue
                tokens = line.split('\t')
                output_content.extend([ tokens[i] +'\n' for i in [1,2]])

    print("Generated %d question answer pairs" % (len(output_content) / 2,))

    with open(OUTPUT_FILE, 'wt') as f:
        f.writelines(output_content)

    cleanup()
