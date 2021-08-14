import json
import boto3

def lambda_handler(event, context):
    
    session = boto3.Session()
    honeycode_client = session.client('honeycode', region_name = 'us-west-2')
    
    response = honeycode_client.batch_update_table_rows(
        workbookId = 'ecf97bf3-57d1-48c2-a1e4-2886fe4df3ff',
        tableId = '654cf941-7739-40fc-a43e-c8609132c9c5',
        rowsToUpdate = [
            {
              "rowId": "row:654cf941-7739-40fc-a43e-c8609132c9c5/880de4ef-9db0-34fd-ad54-dc6675aed194",
              "cellsToUpdate": {
                "f8526acf-bc65-4d2c-ab39-755f520fff56": { "fact": str(event) }
              }
            }
        ])
    
    return {
        'statusCode': 200,
        'body': json.dumps('Hello from Lambda!')
    }
    