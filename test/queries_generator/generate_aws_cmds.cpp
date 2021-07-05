#include <iostream>
#include <fstream>

using namespace std;

int main()
{
    fstream query_file, cmd_file;
    query_file.open("queries.txt", ios::in);
    cmd_file.open("aws_cmds.sh", ios::out);
    cmd_file << "#!/bin/sh\nset -x\nset -e\n\n";
    cmd_file << "mkdir -p aws_results\n";
    string bucket, csv_file, query, aws_cmd;
    cout << "Enter bucket name: ";
    cin >> bucket;
    cout << "Enter file name: ";
    cin >> csv_file;
    for(int i = 1; getline(query_file, query); i++)
    {
        aws_cmd = "aws s3api select-object-content --bucket " + bucket + " --key " + csv_file + "  --expression-type \'SQL\' --input-serialization \'{\"CSV\": {}, \"CompressionType\": \"NONE\"}\' --output-serialization \'{\"CSV\": {}}\' --profile openshift-dev --expression \"" + query + "\" \"aws_results/output" + to_string(i) + ".csv\"";
	cmd_file << aws_cmd << endl;
    }
    cmd_file.close();
    query_file.close();
    return 0;
}
